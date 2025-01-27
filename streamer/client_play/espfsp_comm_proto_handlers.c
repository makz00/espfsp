/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

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
        instance->session_data.stream_started = false;
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
    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        if (instance->session_data.stream_started == true)
        {
            ret = espfsp_data_proto_stop(&instance->data_proto);
        }
        if (ret == ESP_OK)
        {
            instance->session_data.stream_started = false;
        }
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_client_play_resp_session_ack_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (!instance->session_data.active)
    {
        instance->session_data.active = true;
        instance->session_data.session_id = msg->session_id;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_client_play_resp_sources_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_resp_sources_resp_message_t *msg = (espfsp_comm_resp_sources_resp_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    espfsp_get_sources_data_t sources_data;
    bool should_handle_response = false;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        should_handle_response = true;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    if (should_handle_response)
    {
        sources_data.sources_names_len = msg->num_sources;
        memcpy(sources_data.sources_names_buf, msg->source_names, sizeof(sources_data.sources_names_buf));

        if (xQueueSend(instance->get_sources_data_queue, &sources_data, 5) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot send produced value. First element from FIFO will be dropped");
            espfsp_get_sources_data_t sources_data_to_drop;

            if (xQueueReceive(instance->get_sources_data_queue, &sources_data_to_drop, 0) != pdTRUE)
            {
                ESP_LOGI(TAG, "Cannot drop first element from FIFO");
            }
            if (xQueueSend(instance->get_sources_data_queue, &sources_data, 5) != pdTRUE)
            {
                ESP_LOGE(TAG, "Cannot send produced value after drop");
                ret = ESP_FAIL;
            }
        }
    }

    return ret;
}

esp_err_t espfsp_client_play_resp_frame_config_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_resp_frame_params_resp_message_t *msg = (espfsp_comm_resp_frame_params_resp_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    espfsp_get_param_data_t param_data;
    bool should_handle_response = false;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        should_handle_response = true;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    if (should_handle_response)
    {
        param_data.param_id = msg->param_id;
        param_data.param_value = msg->value;

        if (xQueueSend(instance->get_frame_config_data_queue, &param_data, 5) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot send produced value. First element from FIFO will be dropped");
            espfsp_get_param_data_t param_data_to_drop;

            if (xQueueReceive(instance->get_frame_config_data_queue, &param_data_to_drop, 0) != pdTRUE)
            {
                ESP_LOGI(TAG, "Cannot drop first element from FIFO");
            }
            if (xQueueSend(instance->get_frame_config_data_queue, &param_data, 5) != pdTRUE)
            {
                ESP_LOGE(TAG, "Cannot send produced value after drop");
                ret = ESP_FAIL;
            }
        }
    }

    return ret;
}

esp_err_t espfsp_client_play_resp_cam_config_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_resp_cam_params_resp_message_t *msg = (espfsp_comm_resp_cam_params_resp_message_t *) msg_content;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) ctx;

    espfsp_get_param_data_t param_data;
    bool should_handle_response = false;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }
    if (instance->session_data.active && instance->session_data.session_id == msg->session_id)
    {
        should_handle_response = true;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    if (should_handle_response)
    {
        param_data.param_id = msg->param_id;
        param_data.param_value = msg->value;

        if (xQueueSend(instance->get_cam_config_data_queue, &param_data, 5) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot send produced value. First element from FIFO will be dropped");
            espfsp_get_param_data_t param_data_to_drop;

            if (xQueueReceive(instance->get_cam_config_data_queue, &param_data_to_drop, 0) != pdTRUE)
            {
                ESP_LOGI(TAG, "Cannot drop first element from FIFO");
            }
            if (xQueueSend(instance->get_cam_config_data_queue, &param_data, 5) != pdTRUE)
            {
                ESP_LOGE(TAG, "Cannot send produced value after drop");
                ret = ESP_FAIL;
            }
        }
    }

    return ret;
}

esp_err_t espfsp_client_play_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx)
{
    esp_err_t ret = ESP_OK;
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
        instance->session_data.active = false;
        instance->session_data.session_id = -1;
        instance->session_data.stream_started = false;
    }
    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ret;
}
