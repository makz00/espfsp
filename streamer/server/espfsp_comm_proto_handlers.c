/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_session_manager.h"
#include "server/espfsp_comm_proto_handlers.h"

static const char *TAG = "SERVER_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t espfsp_server_req_session_init_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_init_message_t *msg = (espfsp_comm_proto_req_session_init_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_resp_session_ack_message_t resp;
    uint32_t session_id = -123;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_activate_session(session_manager, comm_proto);
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &session_id);
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK)
    {
        resp.session_id = session_id;
        ret = espfsp_comm_proto_session_ack(comm_proto, &resp);
    }

    return ret;
}

esp_err_t espfsp_server_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    uint32_t session_id = -123;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &session_id);
        if (ret == ESP_OK && session_id == msg->session_id)
        {
            ret = espfsp_session_manager_deactivate_session(session_manager, comm_proto);
        }
        espfsp_session_manager_release(session_manager);
    }

    return ret;
}

static esp_err_t handle_client_play_session_stream(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    uint32_t received_session_id,
    espfsp_comm_proto_t **primary_push_comm_proto,
    uint32_t *primary_push_session_id,
    bool stream_state_to_set)
{
    uint32_t play_session_id = -123;
    *primary_push_comm_proto = NULL;
    *primary_push_session_id = -123;

    esp_err_t ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id == received_session_id)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, primary_push_comm_proto);
        }
        if (ret == ESP_OK && *primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, *primary_push_comm_proto, primary_push_session_id);
        }
        if (ret == ESP_OK && *primary_push_comm_proto != NULL && *primary_push_session_id != -123)
        {
            ret = espfsp_session_manager_set_stream_state(
                session_manager, *primary_push_comm_proto, stream_state_to_set);
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_stream_state(session_manager, comm_proto, stream_state_to_set);
            }
        }

        espfsp_session_manager_release(session_manager);
    }

    return ret;
}

esp_err_t espfsp_server_req_start_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *received_msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_req_start_stream_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t primary_push_session_id = -123;

    ret = handle_client_play_session_stream(
        session_manager,
        comm_proto,
        received_msg->session_id,
        &primary_push_comm_proto,
        &primary_push_session_id,
        false);

    if (ret == ESP_OK && primary_push_comm_proto != NULL && primary_push_session_id != -123)
    {
        send_msg.session_id = primary_push_session_id;
        ret = espfsp_comm_proto_start_stream(primary_push_comm_proto, &send_msg);
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_start(&instance->client_push_data_proto);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_start(&instance->client_play_data_proto);
        }
    }

    return ret;
}

esp_err_t espfsp_server_req_stop_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *received_msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_req_stop_stream_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t primary_push_session_id = -123;

    ret = handle_client_play_session_stream(
        session_manager,
        comm_proto,
        received_msg->session_id,
        &primary_push_comm_proto,
        &primary_push_session_id,
        true);

    if (ret == ESP_OK && primary_push_comm_proto != NULL && primary_push_session_id != -123)
    {
        send_msg.session_id = primary_push_session_id;
        ret = espfsp_comm_proto_stop_stream(primary_push_comm_proto, &send_msg);
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_stop(&instance->client_push_data_proto);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_stop(&instance->client_play_data_proto);
        }
    }

    return ret;
}

esp_err_t espfsp_server_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_req_stop_stream_message_t other_primary_session_send_msg;
    espfsp_comm_proto_t *other_primary_session_comm_proto = NULL;
    uint32_t other_primary_session_session_id = -123;
    bool stream_started = false;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        espfsp_session_manager_session_type_t session_type;
        espfsp_comm_proto_t *given_type_primary_session = NULL;

        ret = espfsp_session_manager_get_session_type(session_manager, comm_proto, &session_type);
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, session_type, &given_type_primary_session);
        }
        // One of primary sessions get closed, so we have to check if streaming was activated
        if (ret == ESP_OK && given_type_primary_session == comm_proto)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, comm_proto, &stream_started);
        }
        // If streaming was activated, but connection get closed, we have to change states in Session Manager
        if (ret == ESP_OK && stream_started)
        {
            espfsp_comm_proto_t *primary_session = NULL;
            ret = espfsp_session_manager_get_primary_session(
                session_manager,
                ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                &primary_session);
            if (ret == ESP_OK && primary_session != NULL)
            {
                ret = espfsp_session_manager_set_stream_state(session_manager, primary_session, false);
            }
            if (ret == ESP_OK && primary_session != comm_proto)
            {
                other_primary_session_comm_proto = primary_session;
            }
            if (ret == ESP_OK)
            {
                primary_session = NULL;
                ret = espfsp_session_manager_get_primary_session(
                    session_manager,
                    ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY,
                    &primary_session);
            }
            if (ret == ESP_OK && primary_session != NULL)
            {
                ret = espfsp_session_manager_set_stream_state(session_manager, primary_session, false);
            }
            if (ret == ESP_OK && primary_session != comm_proto)
            {
                other_primary_session_comm_proto = primary_session;
            }
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_deactivate_session(session_manager, comm_proto);
        }

        espfsp_session_manager_release(session_manager);
    }
    // If streaming was activated, but connection get closed, we have to stop data task and inform other activated session
    // to not send/receive data
    if (stream_started)
    {
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, other_primary_session_comm_proto, &other_primary_session_session_id);
        }
        if (ret == ESP_OK)
        {
            other_primary_session_send_msg.session_id = -1;
            ret = espfsp_comm_proto_stop_stream(other_primary_session_comm_proto, &other_primary_session_send_msg);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_stop(&instance->client_push_data_proto);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_stop(&instance->client_play_data_proto);
        }
    }

    return ret;
}
