/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_params_map.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_session_manager.h"
#include "server/espfsp_comm_proto_handlers.h"

static const char *TAG = "SERVER_COMMUNICATION_PROTOCOL_HANDLERS";

#define LOG_IF_FAIL(ret) if (ret == ESP_FAIL) { ESP_LOGE(TAG, "Session handling failed. Line: %d", __LINE__); }

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
    LOG_IF_FAIL(ret)
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
    LOG_IF_FAIL(ret)

    return ret;
}

static esp_err_t handle_client_play_session_stream(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    uint32_t received_session_id,
    espfsp_comm_proto_t **primary_push_comm_proto,
    uint32_t *primary_push_session_id,
    bool stream_state_to_set,
    espfsp_frame_config_t *primary_push_frame_config)
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
        if (*primary_push_comm_proto != NULL && *primary_push_session_id != -123)
        {
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_stream_state(
                    session_manager, *primary_push_comm_proto, stream_state_to_set);
                if (ret == ESP_OK)
                {
                    ret = espfsp_session_manager_set_stream_state(session_manager, comm_proto, stream_state_to_set);
                }
            }
            if (ret == ESP_OK && primary_push_frame_config != NULL)
            {
                ret = espfsp_session_manager_get_frame_config(
                    session_manager, *primary_push_comm_proto, primary_push_frame_config);
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    LOG_IF_FAIL(ret)

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
    espfsp_frame_config_t primary_push_frame_config;

    ret = handle_client_play_session_stream(
        session_manager,
        comm_proto,
        received_msg->session_id,
        &primary_push_comm_proto,
        &primary_push_session_id,
        true,
        &primary_push_frame_config);

    ESP_LOGE(TAG, "%d", __LINE__);

    LOG_IF_FAIL(ret)
    if (ret == ESP_OK && primary_push_comm_proto != NULL && primary_push_session_id != -123)
    {
        ESP_LOGE(TAG, "%d", __LINE__);
        send_msg.session_id = primary_push_session_id;
        ret = espfsp_comm_proto_start_stream(primary_push_comm_proto, &send_msg);
        if (ret == ESP_OK)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = espfsp_data_proto_start(&instance->client_push_data_proto);
        }
        if (ret == ESP_OK)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = espfsp_data_proto_set_frame_params(&instance->client_play_data_proto, &primary_push_frame_config);
        }
        if (ret == ESP_OK)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
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
        false,
        NULL);

    LOG_IF_FAIL(ret)
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

esp_err_t espfsp_server_req_cam_set_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_cam_set_params_message_t *received_msg = (espfsp_comm_req_cam_set_params_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_req_cam_set_params_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t primary_push_session_id = -123;
    uint32_t play_session_id = -123;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id == received_msg->session_id)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }

        espfsp_session_manager_release(session_manager);
    }
    LOG_IF_FAIL(ret)
    if (ret == ESP_OK && primary_push_comm_proto != NULL && primary_push_session_id != -123)
    {
        send_msg.session_id = primary_push_session_id;
        send_msg.param_id = received_msg->param_id;
        send_msg.value = received_msg->value;

        ret = espfsp_comm_proto_cam_set_params(primary_push_comm_proto, &send_msg);
    }

    return ret;
}

esp_err_t espfsp_server_req_cam_get_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_cam_get_params_message_t *received_msg = (espfsp_comm_req_cam_get_params_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    return ret;
}

static esp_err_t set_frame_config(espfsp_frame_config_t *frame_config, uint16_t param_id, uint32_t value)
{
    esp_err_t ret = ESP_OK;

    espfsp_params_map_frame_param_t param;
    ret = espfsp_params_map_frame_param_get_param(param_id, &param);
    if (ret == ESP_OK)
    {
        switch (param)
        {
        case ESPFSP_PARAM_MAP_FRAME_PIXEL_FORMAT:
            frame_config->pixel_format = (espfsp_pixformat_t) value;
            ESP_LOGI(TAG, "Read pixel_format: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FRAME_SIZE:
            frame_config->frame_size = (espfsp_framesize_t) value;
            ESP_LOGI(TAG, "Read frame_size: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN:
            frame_config->frame_max_len = (uint32_t) value;
            ESP_LOGI(TAG, "Read frame_max_len: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FPS:
            frame_config->fps = (uint16_t) value;
            ESP_LOGI(TAG, "Read fps: %ld", value);
            break;
        default:
            ESP_LOGE(TAG, "Not handled frame parameter");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_server_req_frame_set_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_frame_set_params_message_t *received_msg = (espfsp_comm_req_frame_set_params_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_req_frame_set_params_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t primary_push_session_id = -123;
    uint32_t play_session_id = -123;
    espfsp_frame_config_t primary_push_frame_config;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id == received_msg->session_id)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }
        if (primary_push_comm_proto != NULL && primary_push_session_id != -123)
        {
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_get_frame_config(
                    session_manager, primary_push_comm_proto, &primary_push_frame_config);
            }
            if (ret == ESP_OK)
            {
                ret = set_frame_config(&primary_push_frame_config, received_msg->param_id, received_msg->value);
            }
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_frame_config(
                    session_manager, primary_push_comm_proto, &primary_push_frame_config);
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    LOG_IF_FAIL(ret)
    if (primary_push_comm_proto != NULL && primary_push_session_id != -123)
    {
        if (ret == ESP_OK)
        {
            send_msg.session_id = primary_push_session_id;
            send_msg.param_id = received_msg->param_id;
            send_msg.value = received_msg->value;

            ret = espfsp_comm_proto_frame_set_params(primary_push_comm_proto, &send_msg);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_set_frame_params(&instance->client_play_data_proto, &primary_push_frame_config);
        }
    }

    return ret;
}

esp_err_t espfsp_server_req_frame_get_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_frame_get_params_message_t *received_msg = (espfsp_comm_req_frame_get_params_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    return ret;
}

esp_err_t espfsp_server_req_source_set_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_source_set_message_t *received_msg = (espfsp_comm_req_source_set_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_t *cur_primary_push_comm_proto = NULL;
    espfsp_comm_proto_t *new_primary_push_comm_proto = NULL;
    uint32_t play_session_id = -123;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id == received_msg->session_id)
        {
            bool cur_stream_state = false;

            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &cur_primary_push_comm_proto);
            if (ret == ESP_OK && cur_primary_push_comm_proto != NULL)
            {
                ESP_LOGI(TAG, "%d", __LINE__);

                ret = espfsp_session_manager_get_stream_state(
                    session_manager, cur_primary_push_comm_proto, &cur_stream_state);
            }
            ESP_LOGI(TAG, "%d", __LINE__);
            if (cur_stream_state == false)
            {
                ESP_LOGI(TAG, "%d", __LINE__);
                if (ret == ESP_OK)
                {
                    ESP_LOGI(TAG, "src name: %s", received_msg->source_name);
                    ret = espfsp_session_manager_get_active_session(
                        session_manager,
                        ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                        received_msg->source_name,
                        &new_primary_push_comm_proto);
                }
                if (ret == ESP_OK && new_primary_push_comm_proto != NULL)
                {
                    ESP_LOGI(TAG, "%d", __LINE__);
                    ret = espfsp_session_manager_set_primary_session(
                        session_manager,
                        ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                        new_primary_push_comm_proto);
                }
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    LOG_IF_FAIL(ret)

    return ret;
}

esp_err_t espfsp_server_req_source_get_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_source_get_message_t *received_msg = (espfsp_comm_req_source_get_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_resp_sources_resp_message_t send_msg;
    uint32_t play_session_id = -123;
    espfsp_comm_proto_t *active_push_comm_protos_buf[3];
    int active_push_comm_protos_count;
    uint8_t num_sources = 0;
    char source_names[3][ESPFSP_SERVER_SESSION_NAME_MAX_LEN];

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id == received_msg->session_id)
        {
            ret = espfsp_session_manager_get_active_sessions(
                session_manager,
                ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                active_push_comm_protos_buf,
                3,
                &active_push_comm_protos_count);

            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "received src %d", active_push_comm_protos_count);

                for (int i = 0; i < active_push_comm_protos_count; i++)
                {
                    if (ret == ESP_OK)
                    {
                        ret = espfsp_session_manager_get_session_name(
                            session_manager, active_push_comm_protos_buf[i], source_names[i]);

                        ESP_LOGI(TAG, "src name %s: ", source_names[i]);
                    }
                }
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    LOG_IF_FAIL(ret)
    if (ret == ESP_OK)
    {
        send_msg.session_id = play_session_id;
        send_msg.num_sources = (uint8_t) active_push_comm_protos_count;
        memcpy(send_msg.source_names, source_names, sizeof(source_names));

        ret = espfsp_comm_proto_sources(comm_proto, &send_msg);
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
    LOG_IF_FAIL(ret)
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
            other_primary_session_send_msg.session_id = other_primary_session_session_id;
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
