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

#include "espfsp_client_push.h"
#include "client_push/espfsp_state_def.h"
#include "client_push/espfsp_comm_proto_conf.h"
#include "client_push/espfsp_data_proto_conf.h"
#include "client_common/espfsp_data_task.h"
#include "client_common/espfsp_session_and_control_task.h"

static const char *TAG = "ESPFSP_CLIENT_PUSH";

static espfsp_client_push_state_t *state_ = NULL;

static esp_err_t start_session_and_control_task(espfsp_client_push_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_client_session_and_control_task_data_t *data = (espfsp_client_session_and_control_task_data_t *) malloc(
        sizeof(espfsp_client_session_and_control_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for session and control task data");
        return ESP_FAIL;
    }

    data->comm_proto = &instance->comm_proto;
    data->client_type = ESPFSP_COMM_REQ_CLIENT_PUSH;
    data->local_port = instance->config->local.control_port;
    data->remote_port = instance->config->remote.control_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreate(
        espfsp_client_session_and_control_task,
        "espfsp_client_push_session_and_control_task",
        instance->config->session_and_control_task_info.stack_size,
        (void *) data,
        instance->config->session_and_control_task_info.task_prio,
        &instance->session_and_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start session and control task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_data_task(espfsp_client_push_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_client_data_task_data_t *data = (espfsp_client_data_task_data_t *) malloc(
        sizeof(espfsp_client_data_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for client play data task");
        return ESP_FAIL;
    }

    data->data_proto = &instance->data_proto;
    data->local_port = instance->config->local.data_port;
    data->remote_port = instance->config->remote.data_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreate(
        espfsp_client_data_task,
        "espfsp_client_push_data_task",
        instance->config->data_task_info.stack_size,
        (void *) data,
        instance->config->data_task_info.task_prio,
        &instance->data_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static espfsp_client_push_instance_t *create_new_client_push(const espfsp_client_push_config_t *config)
{
    espfsp_client_push_instance_t *instance = NULL;

    for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES; i++)
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

    instance->config = (espfsp_client_push_config_t *) malloc(sizeof(espfsp_client_push_config_t));
    if (instance->config == NULL)
    {
        ESP_LOGE(TAG, "Config is not initialized");
        return NULL;
    }

    memcpy(instance->config, config, sizeof(espfsp_client_push_config_t));

    instance->session_data.session_id = -123;
    instance->session_data.active = false;
    instance->session_data.camera_started = false;

    esp_err_t err = ESP_OK;

    err = espfsp_client_push_comm_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = espfsp_client_push_data_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = start_session_and_control_task(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = start_data_task(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    return instance;
}

static esp_err_t stop_tasks(espfsp_client_push_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    espfsp_data_proto_stop(&instance->data_proto);
    espfsp_comm_proto_stop(&instance->comm_proto);

    // Wait for task to stop
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    vTaskDelete(instance->data_task_handle);
    vTaskDelete(instance->session_and_control_task_handle);

    return ret;
}

static esp_err_t remove_client_push(espfsp_client_push_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    bool is_given_instance_correct = false;
    for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES; i++)
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
        return ret;
    }

    ret = espfsp_client_push_data_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_client_push_comm_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    free(instance->config);
    instance->used = false;

    return ret;
}

espfsp_client_push_handler_t espfsp_client_push_init(const espfsp_client_push_config_t *config)
{
    if (state_ == NULL)
    {
        state_ = (espfsp_client_push_state_t *) malloc(sizeof(espfsp_client_push_state_t));
        if (state_ == NULL)
        {
            ESP_LOGE(TAG, "State initialization failed");
            return NULL;
        }

        for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES; i++)
        {
            state_->instances[i].used = false;
        }
    }

    espfsp_client_push_handler_t handler = (espfsp_client_push_handler_t) create_new_client_push(config);
    if (handler == NULL)
    {
        ESP_LOGE(TAG, "Client push creation failed");
    }

    return handler;
}

void espfsp_client_push_deinit(espfsp_client_push_handler_t handler)
{
    if (state_ == NULL)
    {
        ESP_LOGE(TAG, "Client push state is not initialized");
        return;
    }

    esp_err_t err = remove_client_push((espfsp_client_push_instance_t *) handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Client push removal failed");
    }
}
