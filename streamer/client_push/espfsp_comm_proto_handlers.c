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
#include "client_push/espfsp_state_def.h"
#include "client_push/espfsp_comm_proto_handlers.h"

static const char *TAG = "CLIENT_PUSH_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t espfsp_client_push_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        instance->session_data.active = false;
        instance->session_data.session_id = -1;
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
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && instance->session_data.camera_started == false)
    {
        ret = instance->config->cb.start_cam(&instance->config->cam_config, &instance->config->frame_config);
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
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && instance->session_data.camera_started == true)
    {
        ret = espfsp_data_proto_stop(&instance->data_proto);
        if (ret == ESP_OK)
        {
            ret = instance->config->cb.stop_cam();
        }
        if (ret == ESP_OK)
        {
            instance->session_data.camera_started = false;
        }
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
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
    {
        instance->session_data.active = true;
        instance->session_data.session_id = msg->session_id;
    }

    return ret;
}

esp_err_t espfsp_client_push_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    espfsp_comm_proto_req_session_init_message_t msg = {
        .client_type = ESPFSP_COMM_REQ_CLIENT_PUSH,
    };

    if (instance->session_data.camera_started == true)
    {
        ret = espfsp_data_proto_stop(&instance->data_proto);
        if (ret == ESP_OK)
        {
            ret = instance->config->cb.stop_cam();
        }

        instance->session_data.camera_started = false;
    }

    return ret;
}
