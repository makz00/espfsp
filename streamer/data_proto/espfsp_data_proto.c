/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stddef.h>

#include "espfsp_frame_config.h"
#include "data_proto/espfsp_data_recv_proto.h"
#include "data_proto/espfsp_data_send_proto.h"
#include "data_proto/espfsp_data_proto.h"

#define QUEUE_MAX_SIZE 3

#define NO_VAL 0
#define START_VAL 1
#define STOP_VAL 2

static const char *TAG = "ESPFSP_DATA_PROTOCOL";

static esp_err_t update_frame_config(espfsp_data_proto_t *data_proto, espfsp_frame_config_t *frame_config)
{
    // Probably only FPS will be needed, but for now lets keep all frame parameters
    data_proto->frame_config.pixel_format = frame_config->pixel_format;
    data_proto->frame_config.frame_size = frame_config->frame_size;
    data_proto->frame_config.frame_max_len = frame_config->frame_max_len;
    data_proto->frame_config.fps = frame_config->fps;

    data_proto->frame_interval_us = 1000000ULL / frame_config->fps;

    ESP_LOGI(TAG, "FPS updated to: %d", frame_config->fps); // Debug only

    return ESP_OK;
}

esp_err_t espfsp_data_proto_init(espfsp_data_proto_t *data_proto, espfsp_data_proto_config_t *config)
{
    data_proto->config = (espfsp_data_proto_config_t *) malloc(sizeof(espfsp_data_proto_config_t));
    if (!data_proto->config)
    {
        ESP_LOGE(TAG, "Cannot initialize memory for config");
        return ESP_FAIL;
    }

    memcpy(data_proto->config, config, sizeof(espfsp_data_proto_config_t));

    data_proto->startStopQueue = xQueueCreate(QUEUE_MAX_SIZE, sizeof(uint8_t));
    if (data_proto->startStopQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize start-stop queue");
        return ESP_FAIL;
    }

    data_proto->settingsQueue = xQueueCreate(QUEUE_MAX_SIZE, sizeof(espfsp_frame_config_t));
    if (data_proto->settingsQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize settings queue");
        return ESP_FAIL;
    }

    return update_frame_config(data_proto, config->frame_config);
}

esp_err_t espfsp_data_proto_deinit(espfsp_data_proto_t *data_proto)
{
    free(data_proto->config);
    vQueueDelete(data_proto->startStopQueue);
    vQueueDelete(data_proto->settingsQueue);

    return ESP_OK;
}

static esp_err_t handle_data_proto(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;

    switch (data_proto->config->type)
    {
    case ESPFSP_DATA_PROTO_TYPE_SEND:

        ret = espfsp_data_proto_handle_send(data_proto, sock);
        break;

    case ESPFSP_DATA_PROTO_TYPE_RECV:

        ret = espfsp_data_proto_handle_recv(data_proto, sock);
        break;

    default:

        ESP_LOGE(TAG, "Data protocol type not handled");
        return ESP_FAIL;
    }

    return ret;
}

static void change_state_base_ret(espfsp_data_proto_t *data_proto, espfsp_data_proto_state_t next_state, esp_err_t ret_val)
{
    if (ret_val != ESP_OK)
        data_proto->state = ESPFSP_DATA_PROTO_STATE_ERROR;
    else
        data_proto->state = next_state;
}

esp_err_t espfsp_data_proto_run(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;

    espfsp_data_proto_state_t next_state;
    ESP_LOGI(TAG, "Start data handling");

    data_proto->state = ESPFSP_DATA_PROTO_STATE_START_STOP_CHECK;
    data_proto->last_traffic = NO_SIGNAL;
    data_proto->en = 1;

    bool started = false;

    while (data_proto->en)
    {
        switch (data_proto->state)
        {
        case ESPFSP_DATA_PROTO_STATE_START_STOP_CHECK:

            next_state = ESPFSP_DATA_PROTO_STATE_START_STOP_CHECK;
            uint8_t queue_val = NO_VAL;

            if (xQueueReceive(data_proto->startStopQueue, &queue_val, 0) == pdPASS)
            {
                if (queue_val == START_VAL)
                {
                    started = true;
                    ESP_LOGI(TAG, "Start has been read!!!"); // Debug only
                    next_state = ESPFSP_DATA_PROTO_STATE_SETTING;
                }
                else if (queue_val == STOP_VAL)
                {
                    started = false;
                    ESP_LOGI(TAG, "Stop has been read!!!"); // Debug only
                    next_state = ESPFSP_DATA_PROTO_STATE_START_STOP_CHECK;
                }
                else
                {
                    ESP_LOGE(TAG, "Start value not handled");
                    ret = ESP_FAIL;
                }
            }
            else if (started)
            {
                next_state = ESPFSP_DATA_PROTO_STATE_SETTING;
            }
            else
            {
                taskYIELD();
            }

            change_state_base_ret(data_proto, next_state, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_SETTING:

            espfsp_frame_config_t frame_config;
            if (xQueueReceive(data_proto->settingsQueue, &frame_config, 0) == pdPASS)
            {
                ret = update_frame_config(data_proto, &frame_config);
            }

            change_state_base_ret(data_proto, ESPFSP_DATA_PROTO_STATE_LOOP, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_LOOP:

            ret = handle_data_proto(data_proto, sock);
            change_state_base_ret(data_proto, ESPFSP_DATA_PROTO_STATE_CONTROL, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_CONTROL:

            // Check if sending parameters then publish them for interested sides
            change_state_base_ret(data_proto, ESPFSP_DATA_PROTO_STATE_START_STOP_CHECK, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_RETURN:

            ESP_LOGI(TAG, "Stop data handling");
            return ESP_OK;

        case ESPFSP_DATA_PROTO_STATE_ERROR:

            ESP_LOGE(TAG, "Data protocol failed");
            return ESP_FAIL;

        default:

            ESP_LOGE(TAG, "Data protocol state not handled");
            data_proto->state = ESPFSP_DATA_PROTO_STATE_ERROR;
            break;
        }
    }

    return ESP_OK;
}

esp_err_t espfsp_data_proto_start(espfsp_data_proto_t *data_proto)
{
    uint8_t start_val = START_VAL;
    if (xQueueSend(data_proto->startStopQueue, &start_val, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot send start value to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_data_proto_stop(espfsp_data_proto_t *data_proto)
{
    uint8_t stop_val = STOP_VAL;
    if (xQueueSend(data_proto->startStopQueue, &stop_val, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot send stop value to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_data_proto_set_frame_params(espfsp_data_proto_t *data_proto, espfsp_frame_config_t *frame_config)
{
    xQueueReset(data_proto->settingsQueue);

    if (xQueueSend(data_proto->settingsQueue, frame_config, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot send frame config to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}
