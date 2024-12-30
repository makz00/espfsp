/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_message_defs.h"
#include "espfsp_message_buffer.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_client_play_data_task.h"
#include "server/espfsp_client_push_data_task.h"
#include "server/espfsp_client_play_session_and_control_task.h"
#include "server/espfsp_client_push_session_and_control_task.h"

static const char *TAG = "ESPFSP_SERVER";

static espfsp_server_state_t *state_ = NULL;

static esp_err_t initialize_synchronizers(espfsp_server_instance_t * instance)
{
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

    ret = start_client_push_data_task(instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data client push task has not been created successfully");
        return ret;
    }

    ret = start_client_push_session_and_control_task(instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Session and control client push task has not been created successfully");
        return ret;
    }

    ret = start_client_play_data_task(instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data client play task has not been created successfully");
        return ret;
    }

    ret = start_client_play_session_and_control_task(instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Session and control client play task has not been created successfully");
        return ret;
    }

    return ret;
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

    espfsp_receiver_buffer_config_t receiver_buffer_config = {
        .buffered_fbs = config->buffered_fbs,
        .frame_max_len = config->frame_config.frame_max_len,
    };

    err = espfsp_message_buffer_init(&instance->receiver_buffer, &receiver_buffer_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Buffer initialization failed");
        return NULL;
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

static esp_err_t stop_tasks(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    // inform sender task to stop sending data
    // inform session and control task to close the session

    vTaskDelete(instance->client_push_handlers[0].data_task_handle);
    vTaskDelete(instance->client_push_handlers[0].session_and_control_task_handle);
    vTaskDelete(instance->client_play_handlers[0].data_task_handle);
    vTaskDelete(instance->client_play_handlers[0].session_and_control_task_handle);

    return ret;
}

static esp_err_t deinitialize_synchronizers(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    return ret;
}

static esp_err_t remove_server(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    bool is_given_instance_correct = false;
    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_INSTANCES; i++)
    {
        if (&state_->instances[i] == instance)
        {
            is_given_instance_correct = true;
        }
    }

    if (is_given_instance_correct == false)
    {
        ESP_LOGE(TAG, "Given instance pointer is incorrect");
        return ESP_FAIL;
    }

    ret = stop_tasks(instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Stop tasks failed");
        return ret;
    }

    ret = deinitialize_synchronizers(instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Deinitialization of synchronizers failed");
        return ret;
    }

    ret = espfsp_message_buffer_deinit(&instance->receiver_buffer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Buffer initialization failed");
        return NULL;
    }

    free(instance->config);
    instance->used = false;

    return ret;
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
    if (state_ == NULL)
    {
        ESP_LOGE(TAG, "Server state is not initialized");
        return;
    }

    esp_err_t err = remove_server((espfsp_server_instance_t *) handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Server removal failed");
    }
}
