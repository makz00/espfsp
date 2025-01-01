/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "server/espfsp_comm_proto_handlers.h"

static const char *TAG = "SERVER_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t handle_session_init_for_client_push()
{
    esp_err_t ret = ESP_OK;

    return ret;
}

esp_err_t handle_session_init_for_client_play()
{
    esp_err_t ret = ESP_OK;

    return ret;
}

esp_err_t req_session_init_server_handler(void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_init_message_t *msg = (espfsp_comm_proto_req_session_init_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    switch (msg->client_type)
    {
    case ESPFSP_COMM_REQ_CLIENT_PUSH:

        ret = handle_session_init_for_client_push(instance);
        break;

    case ESPFSP_COMM_REQ_CLIENT_PLAY:

        ret = handle_session_init_for_client_play(instance);
        break;

    default:

        ESP_LOGE(TAG, "Unsupported type of client");
        return ESP_FAIL;
    }

    return ret;
}

esp_err_t req_session_terminate_server_handler(void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    return ret;
}

esp_err_t req_session_ping_server_handler(void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_ping_message_t *msg = (espfsp_comm_proto_req_session_ping_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    return ret;
}

esp_err_t req_start_stream_server_handler(void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    return ret;
}

esp_err_t req_stop_stream_server_handler(void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    return ret;
}
