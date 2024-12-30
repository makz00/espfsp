/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>
#include <stdalign.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "sys/time.h"
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_message_defs.h"
#include "server/espfsp_server_state_def.h"
#include "server/espfsp_client_play_data_task.h"
#include "server/espfsp_client_push_data_task.h"
#include "server/espfsp_client_play_session_and_control_task.h"
#include "server/espfsp_client_push_session_and_control_task.h"

static const char *TAG = "ESPFSP_SERVER";

static espfsp_server_state_t *state_ = NULL;

static esp_err_t espfsp_buffers_init(espfsp_server_instance_t *instance)
{
    instance->fbs_messages_buf = (espfsp_message_assembly_t *) heap_caps_aligned_calloc(
        alignof(espfsp_message_assembly_t),
        1,
        instance->config->buffered_fbs * sizeof(espfsp_message_assembly_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    if (!instance->fbs_messages_buf)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf");
        return ESP_FAIL;
    }

    size_t alloc_size;

    // HD resolution: 1280*720. In esp_cam it is diviced by 5. Here Resolution is diviced by 3;
    // alloc_size = 307200;

    // alloc_size = 100000;
    alloc_size = instance->config->frame_config.frame_max_len;

    // It is safe to assume that this size packet will not be fragmented
    // size_t max_udp_packet_size = 512;

    // It is 307200 / 512
    // int number_of_messages_assembly = 600;

    for (int i = 0; i < instance->config->buffered_fbs; i++)
    {
        instance->fbs_messages_buf[i].buf = (uint8_t *) heap_caps_malloc(
            alloc_size * sizeof(uint8_t),
            MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

        if (!instance->fbs_messages_buf[i].buf)
        {
            ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf[i].buf for i=%d", i);
            return ESP_FAIL;
        }

        instance->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    instance->s_fb = (espfsp_fb_t *) heap_caps_malloc(sizeof(espfsp_fb_t), MALLOC_CAP_DEFAULT);

    if (!instance->s_fb)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fb");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t initialize_synchronizers(espfsp_server_instance_t * instance)
{
    instance->frameQueue = xQueueCreate(instance->config->buffered_fbs, sizeof(espfsp_message_assembly_t*));
    if (instance->frameQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize message assembly queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_push_data_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    xStatus = xTaskCreatePinnedToCore(
        espfsp_server_client_push_data_task,
        "espfsp_server_client_push_data_task",
        instance->config->client_push_data_task_info.stack_size,
        NULL,
        instance->config->client_push_data_task_info.task_prio,
        &instance->client_push_handlers[0].data_task_handle,
        1);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_push_session_and_control_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        espfsp_server_client_push_session_and_control_task,
        "espfsp_server_client_push_session_and_control_task",
        instance->config->client_push_session_and_control_task_info.stack_size,
        NULL,
        instance->config->client_push_session_and_control_task_info.task_prio,
        &instance->client_push_handlers[0].session_and_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_play_data_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        espfsp_server_client_play_data_task,
        "espfsp_server_client_play_data_task",
        instance->config->client_play_data_task_info.stack_size,
        NULL,
        instance->config->client_play_data_task_info.task_prio,
        &instance->client_play_handlers[0].data_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_play_session_and_control_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        espfsp_server_client_play_session_and_control_task,
        "espfsp_server_client_play_session_and_control_task",
        instance->config->client_play_session_and_control_task_info.stack_size,
        NULL,
        instance->config->client_play_session_and_control_task_info.task_prio,
        &instance->client_play_handlers[0].session_and_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_tasks(espfsp_server_instance_t * instance)
{
    esp_err_t ret = ESP_OK;

    err = start_client_push_data_task(instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Data client push task has not been created successfully");
        return err;
    }

    err = start_client_push_session_and_control_task(instance);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Session and control client push task has not been created successfully");
        return err;
    }

    err = start_client_play_data_task(instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Data client play task has not been created successfully");
        return err;
    }

    err = start_client_play_session_and_control_task(instance);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Session and control client play task has not been created successfully");
        return err;
    }

    return err;
}

static espfsp_server_instance_t *create_new_server(const espfsp_server_config_t *config)
{
    espfsp_server_instance_t *instance = NULL;

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_INSTANCES; i++)
    {
        if (state_->instances[i].used == false)
        {
            state_->instances[i].used = true;
            instance = &state_->instances[i];
            break;
        }
    }

    if (instance == NULL)
    {
        ESP_LOGE(TAG, "No free instance to create client push");
        return NULL;
    }

    instance->config = (espfsp_server_config_t *) malloc(sizeof(espfsp_server_config_t));
    if (instance->config == NULL)
    {
        ESP_LOGE(TAG, "Config is not initialized");
        return NULL;
    }

    memcpy(instance->config, config, sizeof(espfsp_server_config_t));

    esp_err_t err = ESP_OK;

    err = espfsp_buffers_init(instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Buffer initialization failed");
        return ret;
    }

    err = initialize_synchronizers(instance);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Initialization of synchronizers failed");
        return NULL;
    }

    err = start_tasks(instance);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Start tasks failed");
        return NULL;
    }

    return instance;
}

espfsp_server_handler_t espfsp_server_init(const espfsp_server_config_t *config)
{
    if (state_ == NULL)
    {
        state_ = (espfsp_server_state_t *) malloc(sizeof(espfsp_server_state_t));
        if (state_ == NULL)
        {
            ESP_LOGE(TAG, "State initialization failed");
            return NULL;
        }

        for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_INSTANCES; i++)
        {
            state_->instances[i].used = false;
        }
    }

    espfsp_server_handler_t handler = (espfsp_server_handler_t) create_new_server(config);
    if (handler == NULL)
    {
        ESP_LOGE(TAG, "Server creation failed");
    }

    return handler;
}

void espfsp_server_deinit(espfsp_server_handler_t handler)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED!");

    return ESP_FAIL;
}
