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

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_deactivate_session(session_manager, comm_proto);
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

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_primary_session(
            session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }

        espfsp_session_manager_release(session_manager);
    }
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

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_primary_session(
            session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }

        espfsp_session_manager_release(session_manager);
    }
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

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_deactivate_session(session_manager, comm_proto);
        espfsp_session_manager_release(session_manager);
    }

    return ret;
}
