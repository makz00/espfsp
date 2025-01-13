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
#include "client_play/espfsp_state_def.h"
#include "client_play/espfsp_comm_proto_handlers.h"

static const char *TAG = "CLIENT_PLAY_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t espfsp_client_play_req_session_terminate_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        instance->session_data.active = false;
        instance->session_data.session_id = -1;
    }

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_client_play_req_stop_stream_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (instance->session_data.stream_started == true)
    {
        ret = espfsp_data_proto_stop(&instance->data_proto);
    }
    if (ret == ESP_OK)
    {
        instance->session_data.stream_started = false;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_client_play_resp_session_ack_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Cannot handle new session ack. There is already one session");
        return ESP_FAIL;
    }

    instance->session_data.active = true;
    instance->session_data.session_id = msg->session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_client_play_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    espfsp_comm_proto_req_session_init_message_t msg = {
        .client_type = ESPFSP_COMM_REQ_CLIENT_PLAY,
    };

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (instance->session_data.stream_started == true)
    {
        ret = espfsp_data_proto_stop(&instance->data_proto);
    }
    if (ret == ESP_OK)
    {
        instance->session_data.stream_started = false;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ret;
}
