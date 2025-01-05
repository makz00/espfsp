/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "comm_proto/espfsp_comm_proto.h"
#include "client_play/espfsp_state_def.h"
#include "client_play/espfsp_comm_proto_handlers.h"

static const char *TAG = "CLIENT_PLAY_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t espfsp_client_play_req_session_terminate_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY);

    if (instance->session_data.val && instance->session_data.session_id == msg->session_id)
    {
        instance->session_data.val = false;
    }

    xSemaphoreGive(instance->session_data.mutex);

    return ESP_OK;
}

esp_err_t espfsp_client_play_resp_session_ack_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY);

    if (instance->session_data.val)
    {
        xSemaphoreGive(instance->session_data.mutex);
        ESP_LOGE(TAG, "Cannot handle new session ack. There is already one session");
        return ESP_FAIL;
    }

    instance->session_data.val = true;
    instance->session_data.session_id = msg->session_id;
    xSemaphoreGive(instance->session_data.mutex);

    return ESP_OK;
}
