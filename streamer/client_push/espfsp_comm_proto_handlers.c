/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_params_map.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"
#include "client_push/espfsp_state_def.h"
#include "client_push/espfsp_comm_proto_handlers.h"

static const char *TAG = "CLIENT_PUSH_COMMUNICATION_PROTOCOL_HANDLERS";

static bool is_session_active(espfsp_client_push_session_data_t *session_data)
{
    return session_data->active;
}

static bool is_session_active_for_id(espfsp_client_push_session_data_t *session_data, uint32_t received_id)
{
    return is_session_active(session_data) && session_data->session_id == received_id;
}

static bool is_session_camera_started_for_id(espfsp_client_push_session_data_t *session_data, uint32_t received_id)
{
    return is_session_active_for_id(session_data, received_id) && session_data->camera_started;
}

static bool is_session_camera_not_started_for_id(espfsp_client_push_session_data_t *session_data, uint32_t received_id)
{
    return is_session_active_for_id(session_data, received_id) && !session_data->camera_started;
}

static void start_session_camera(espfsp_client_push_session_data_t *session_data)
{
    session_data->camera_started = true;
}

static void activ_session(espfsp_client_push_session_data_t *session_data, uint32_t received_id)
{
    session_data->active = true;
    session_data->session_id = received_id;
}

static void deactiv_session(espfsp_client_push_session_data_t *session_data)
{
    session_data->active = false;
    session_data->session_id = -1;
}

esp_err_t espfsp_client_push_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_active_for_id(&instance->session_data, msg->session_id))
    {
        deactiv_session(&instance->session_data);
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for session termnate");
    }

    return ret;
}

esp_err_t espfsp_client_push_req_start_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *received_msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_camera_not_started_for_id(&instance->session_data, received_msg->session_id))
    {
        ret = instance->config->cb.start_cam(&instance->config->cam_config, &instance->config->frame_config);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Start camera callback failed. Stream will not be started");
            ret = ESP_OK;
        }
        else
        {
            if (ret == ESP_OK)
            {
                ret = espfsp_data_proto_set_frame_params(&instance->data_proto, &instance->config->frame_config);
            }
            if (ret == ESP_OK)
            {
                ret = espfsp_data_proto_start(&instance->data_proto);
            }
            if (ret == ESP_OK)
            {
                start_session_camera(&instance->session_data);
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for start stream");
    }

    return ret;
}

esp_err_t espfsp_client_push_req_stop_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *received_msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_camera_started_for_id(&instance->session_data, received_msg->session_id))
    {
        ret = espfsp_data_proto_stop(&instance->data_proto);
        if (ret == ESP_OK)
        {
            ret = instance->config->cb.stop_cam();
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Stop camera callback failed");
                ret = ESP_OK;
            }
        }
        if (ret == ESP_OK)
        {
            instance->session_data.camera_started = false;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for stop stream");
    }

    return ret;
}

esp_err_t espfsp_client_push_resp_session_ack_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!is_session_active(&instance->session_data))
    {
        activ_session(&instance->session_data, msg->session_id);
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for session ack");
    }

    return ret;
}

esp_err_t espfsp_client_push_req_cam_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_cam_set_params_message_t *received_msg = (espfsp_comm_req_cam_set_params_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_active_for_id(&instance->session_data, received_msg->session_id))
    {
        ret = espfsp_params_map_set_cam_config(
            &instance->config->cam_config, received_msg->param_id, received_msg->value);
        if (ret == ESP_OK)
        {
            ret = instance->config->cb.reconf_cam(&instance->config->cam_config);
        }
        if(ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Reconfiguration camera callback failed");
            ret = ESP_OK;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for cam set params");
    }

    return ret;
}

esp_err_t espfsp_client_push_req_frame_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_frame_set_params_message_t *received_msg = (espfsp_comm_req_frame_set_params_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_active_for_id(&instance->session_data, received_msg->session_id))
    {
        ret = espfsp_params_map_set_frame_config(
            &instance->config->frame_config, received_msg->param_id, received_msg->value);
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_set_frame_params(&instance->data_proto, &instance->config->frame_config);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Received bad request for frame set params");
    }

    return ret;
}

esp_err_t espfsp_client_push_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (is_session_active(&instance->session_data))
    {
        if (instance->session_data.camera_started)
        {
            ret = espfsp_data_proto_stop(&instance->data_proto);
            if (ret == ESP_OK)
            {
                ret = instance->config->cb.stop_cam();
                if (ret != ESP_OK)
                {
                    ESP_LOGW(TAG, "Stop camera callback failed");
                    ret = ESP_OK;
                }
            }
            if (ret == ESP_OK)
            {
                instance->session_data.camera_started = false;
            }
        }

        deactiv_session(&instance->session_data);
    }

    return ret;
}
