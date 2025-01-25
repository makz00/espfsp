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

esp_err_t espfsp_client_push_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.active || instance->session_data.session_id != msg->session_id)
    {
        ESP_LOGE(TAG, "Bad request for session termination");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
    {
        instance->session_data.active = false;
        instance->session_data.session_id = -123;
    }

    return ret;
}

esp_err_t espfsp_client_push_resp_session_ack_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (instance->session_data.active)
    {
        ESP_LOGE(TAG, "Bad request for session ack");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
    {
        instance->session_data.active = true;
        instance->session_data.session_id = msg->session_id;
    }

    return ret;
}

esp_err_t espfsp_client_push_req_start_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *received_msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.active || instance->session_data.session_id != received_msg->session_id)
    {
        ESP_LOGE(TAG, "Bad request for start stream");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && !instance->session_data.camera_started)
    {
        ret = instance->config->cb.start_cam(&instance->config->cam_config, &instance->config->frame_config);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Start camera callback failed. Stream will not be started");
            ret = ESP_FAIL;
        }
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
            instance->session_data.camera_started = true;
        }
    }

    return ret;
}

esp_err_t espfsp_client_push_req_stop_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *received_msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.active || instance->session_data.session_id != received_msg->session_id)
    {
        ESP_LOGE(TAG, "Bad request for stop stream");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && instance->session_data.camera_started)
    {
        ret = espfsp_data_proto_start(&instance->data_proto);
        if (ret == ESP_OK)
        {
            ret = instance->config->cb.start_cam(&instance->config->cam_config, &instance->config->frame_config);
        }
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Stop camera callback failed. Stream will not be started");
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK)
        {
            instance->session_data.camera_started = false;
        }
    }

    return ret;
}

esp_err_t espfsp_client_push_req_cam_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_cam_set_params_message_t *received_msg = (espfsp_comm_req_cam_set_params_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.active || instance->session_data.session_id != received_msg->session_id)
    {
        ESP_LOGE(TAG, "Bad request for reconf camera");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
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
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_client_push_req_frame_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_req_frame_set_params_message_t *received_msg = (espfsp_comm_req_frame_set_params_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.active || instance->session_data.session_id != received_msg->session_id)
    {
        ESP_LOGE(TAG, "Bad request for reconf frame");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
    {
        ret = espfsp_params_map_set_frame_config(
            &instance->config->frame_config, received_msg->param_id, received_msg->value);
        if (ret == ESP_OK)
        {
            ret = espfsp_data_proto_set_frame_params(&instance->data_proto, &instance->config->frame_config);
        }
    }

    return ret;
}

esp_err_t espfsp_client_push_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (instance->session_data.active)
    {
        if (instance->session_data.camera_started)
        {
            ret = espfsp_data_proto_stop(&instance->data_proto);
            if (ret == ESP_OK)
            {
                ret = instance->config->cb.stop_cam();
            }
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Stop camera callback failed");
                ret = ESP_FAIL;
            }
        }

        instance->session_data.active = false;
        instance->session_data.session_id = -123;
        instance->session_data.camera_started = false;
    }

    return ret;
}
