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
    // espfsp_comm_proto_req_session_init_message_t *msg = (espfsp_comm_proto_req_session_init_message_t *) msg_content;
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
        if (session_id != msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_deactivate_session(session_manager, comm_proto);
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
    uint32_t play_session_id = -123;
    uint32_t primary_push_session_id = -123;
    espfsp_frame_config_t primary_push_frame_config;
    bool push_stream_started = false;
    bool play_stream_started = false;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }
        if (ret == ESP_OK && primary_push_session_id == -123)
        {
            ESP_LOGE(TAG, "Session ID not found");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, primary_push_comm_proto, &push_stream_started);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, comm_proto, &play_stream_started);
        }
        if (ret == ESP_OK && push_stream_started != play_stream_started)
        {
            ESP_LOGE(TAG, "Inconsistent stream state");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK && !push_stream_started && !play_stream_started)
        {
            ret = espfsp_session_manager_set_stream_state(session_manager, primary_push_comm_proto, true);
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_stream_state(session_manager, comm_proto, true);
            }
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_get_frame_config(
                    session_manager, primary_push_comm_proto, &primary_push_frame_config);
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK && !push_stream_started && !play_stream_started)
    {
        if (primary_push_frame_config.buffered_fbs != instance->receiver_buffer.config->buffered_fbs ||
            primary_push_frame_config.fb_in_buffer_before_get != instance->receiver_buffer.config->fb_in_buffer_before_get ||
            primary_push_frame_config.frame_max_len != instance->receiver_buffer.config->frame_max_len ||
            primary_push_frame_config.fps != instance->receiver_buffer.config->fps)
        {
            ret = espfsp_message_buffer_deinit(&instance->receiver_buffer);
            if (ret == ESP_OK)
            {
                espfsp_receiver_buffer_config_t receiver_buffer_new_config = {
                    .buffered_fbs = primary_push_frame_config.buffered_fbs,
                    .frame_max_len = primary_push_frame_config.frame_max_len,
                    .fb_in_buffer_before_get = 0,
                    .fps = primary_push_frame_config.fps,
                };

                ret = espfsp_message_buffer_init(&instance->receiver_buffer, &receiver_buffer_new_config);
            }
        }

        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_set_frame_params(&instance->client_push_data_proto, &primary_push_frame_config);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_set_frame_params(&instance->client_play_data_proto, &primary_push_frame_config);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_start(&instance->client_push_data_proto);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_start(&instance->client_play_data_proto);
        }
        if (ret == ESP_OK)
        {
            send_msg.session_id = primary_push_session_id;
            ret = espfsp_comm_proto_start_stream(primary_push_comm_proto, &send_msg);
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
    uint32_t play_session_id = -123;
    bool push_stream_started = false;
    bool play_stream_started = false;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }
        if (ret == ESP_OK && primary_push_session_id == -123)
        {
            ESP_LOGE(TAG, "Session ID not found");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, primary_push_comm_proto, &push_stream_started);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, comm_proto, &play_stream_started);
        }
        if (ret == ESP_OK && push_stream_started != play_stream_started)
        {
            ESP_LOGE(TAG, "Inconsistent stream state");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK &&  push_stream_started && play_stream_started)
        {
            ret = espfsp_session_manager_set_stream_state(session_manager, primary_push_comm_proto, false);
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_stream_state(session_manager, comm_proto, false);
            }
        }

        espfsp_session_manager_release(session_manager);
    }

    if (ret == ESP_OK && push_stream_started && play_stream_started)
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
    espfsp_cam_config_t primary_push_cam_config;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }
        if (ret == ESP_OK && primary_push_session_id == -123)
        {
            ESP_LOGE(TAG, "Session ID not found");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_cam_config(
                session_manager, primary_push_comm_proto, &primary_push_cam_config);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_params_map_set_cam_config(
                &primary_push_cam_config, received_msg->param_id, received_msg->value);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_set_cam_config(
                session_manager, primary_push_comm_proto, &primary_push_cam_config);
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK)
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

    espfsp_comm_resp_cam_params_resp_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t play_session_id = -123;
    espfsp_cam_config_t primary_push_cam_config;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_cam_config(
                session_manager, primary_push_comm_proto, &primary_push_cam_config);
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK)
    {
        ret = espfsp_params_map_get_cam_config_param_val(
            &primary_push_cam_config, received_msg->param_id, &send_msg.value);
    }
    if (ret == ESP_OK)
    {
        send_msg.session_id = play_session_id;
        send_msg.param_id = received_msg->param_id;
        ret = espfsp_comm_proto_cam_params(comm_proto, &send_msg);
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
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_session_id(
                session_manager, primary_push_comm_proto, &primary_push_session_id);
        }
        if (ret == ESP_OK && primary_push_session_id == -123)
        {
            ESP_LOGE(TAG, "Session ID not found");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_frame_config(
                session_manager, primary_push_comm_proto, &primary_push_frame_config);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_params_map_set_frame_config(
                &primary_push_frame_config, received_msg->param_id, received_msg->value);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_set_frame_config(
                session_manager, primary_push_comm_proto, &primary_push_frame_config);
        }

        espfsp_session_manager_release(session_manager);
    }
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

    return ret;
}

esp_err_t espfsp_server_req_frame_get_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_frame_get_params_message_t *received_msg = (espfsp_comm_req_frame_get_params_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_resp_frame_params_resp_message_t send_msg;
    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    uint32_t play_session_id = -123;
    espfsp_frame_config_t primary_push_frame_config;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto == NULL)
        {
            ESP_LOGI(TAG, "No push primary session");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_frame_config(
                session_manager, primary_push_comm_proto, &primary_push_frame_config);
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK)
    {
        ret = espfsp_params_map_get_frame_config_param_val(
            &primary_push_frame_config, received_msg->param_id, &send_msg.value);
    }
    if (ret == ESP_OK)
    {
        send_msg.session_id = play_session_id;
        send_msg.param_id = received_msg->param_id;
        ret = espfsp_comm_proto_frame_params(comm_proto, &send_msg);
    }

    return ret;
}

esp_err_t espfsp_server_req_source_set_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_source_set_message_t *received_msg = (espfsp_comm_req_source_set_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_session_manager_t *session_manager = &instance->session_manager;

    espfsp_comm_proto_t *primary_push_comm_proto = NULL;
    espfsp_comm_proto_t *new_primary_push_comm_proto = NULL;
    uint32_t play_session_id = -123;
    bool push_stream_started = false;
    bool play_stream_started = false;

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_primary_session(
                session_manager, ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH, &primary_push_comm_proto);
        }
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_stream_state(
                session_manager, primary_push_comm_proto, &push_stream_started);
        }
        if (ret == ESP_OK && primary_push_comm_proto != NULL)
        {
            ret = espfsp_session_manager_get_stream_state(
                session_manager, comm_proto, &play_stream_started);
        }
        if (ret == ESP_OK && push_stream_started != play_stream_started)
        {
            ESP_LOGE(TAG, "Inconsistent stream state");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK && !push_stream_started && !play_stream_started)
        {
            ret = espfsp_session_manager_get_active_session(
                session_manager,
                ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                received_msg->source_name,
                &new_primary_push_comm_proto);
            if (ret == ESP_OK && new_primary_push_comm_proto == NULL)
            {
                ESP_LOGI(TAG, "Canont set new push");
                espfsp_session_manager_release(session_manager);
                return ESP_OK;
            }
            if (ret == ESP_OK)
            {
                ret = espfsp_session_manager_set_primary_session(
                    session_manager,
                    ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                    new_primary_push_comm_proto);
            }
        }

        espfsp_session_manager_release(session_manager);
    }

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

    ret = espfsp_session_manager_take(session_manager);
    if (ret == ESP_OK)
    {
        ret = espfsp_session_manager_get_session_id(session_manager, comm_proto, &play_session_id);
        if (ret == ESP_OK && play_session_id != received_msg->session_id)
        {
            ESP_LOGE(TAG, "Session ID does not match");
            espfsp_session_manager_release(session_manager);
            return ESP_OK;
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_session_manager_get_active_sessions(
                session_manager,
                ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
                active_push_comm_protos_buf,
                3,
                &active_push_comm_protos_count);
        }
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Sources count: %d", active_push_comm_protos_count);

            for (int i = 0; i < active_push_comm_protos_count; i++)
            {
                if (ret == ESP_OK)
                {
                    ret = espfsp_session_manager_get_session_name(
                        session_manager, active_push_comm_protos_buf[i], send_msg.source_names[i]);

                    ESP_LOGI(TAG, "Source name %s: ", send_msg.source_names[i]);
                }
            }
        }

        espfsp_session_manager_release(session_manager);
    }
    if (ret == ESP_OK)
    {
        send_msg.session_id = play_session_id;
        send_msg.num_sources = (uint8_t) active_push_comm_protos_count;

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
    bool was_primary = false;
    bool should_stop_stream = false;

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
        if (ret == ESP_OK && given_type_primary_session == comm_proto)
        {
            was_primary = true;
        }
        if (ret == ESP_OK && was_primary)
        {
            ret = espfsp_session_manager_get_stream_state(session_manager, comm_proto, &should_stop_stream);
        }
        if (ret == ESP_OK && should_stop_stream)
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
    if (ret == ESP_OK && should_stop_stream)
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
