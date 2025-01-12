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
#include "espfsp_message_buffer.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_comm_proto_conf.h"
#include "server/espfsp_data_proto_conf.h"
#include "server/espfsp_session_manager.h"
#include "server/espfsp_data_task.h"
#include "server/espfsp_session_and_control_task.h"

static const char *TAG = "ESPFSP_SERVER";

static espfsp_server_state_t *state_ = NULL;

static uint32_t generate_session_id(espfsp_session_manager_session_type_t type)
{
    static uint32_t next_id = 1001;
    return next_id++;
}

static esp_err_t start_client_push_session_and_control_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_server_session_and_control_task_data_t *data = (espfsp_server_session_and_control_task_data_t *) malloc(
        sizeof(espfsp_server_session_and_control_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for session and control task data");
        return ESP_FAIL;
    }

    data->session_manager = &instance->session_manager;
    data->session_type = ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH;
    data->server_port = instance->config->client_push_local.control_port;
    data->connection_task_info.stack_size = instance->config->client_push_session_and_control_task_info.stack_size;
    data->connection_task_info.task_prio = instance->config->client_push_session_and_control_task_info.task_prio;

    xStatus = xTaskCreate(
        espfsp_server_session_and_control_task,
        "espfsp_server_client_push_session_and_control_task",
        instance->config->client_push_session_and_control_task_info.stack_size,
        (void *) data,
        instance->config->client_push_session_and_control_task_info.task_prio,
        &instance->server_client_push_handle);

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

    espfsp_server_session_and_control_task_data_t *data = (espfsp_server_session_and_control_task_data_t *) malloc(
        sizeof(espfsp_server_session_and_control_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for session and control task data");
        return ESP_FAIL;
    }

    data->session_manager = &instance->session_manager;
    data->session_type = ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY;
    data->server_port = instance->config->client_play_local.control_port;
    data->connection_task_info.stack_size = instance->config->client_play_session_and_control_task_info.stack_size;
    data->connection_task_info.task_prio = instance->config->client_play_session_and_control_task_info.task_prio;

    xStatus = xTaskCreate(
        espfsp_server_session_and_control_task,
        "espfsp_server_client_play_session_and_control_task",
        instance->config->client_play_session_and_control_task_info.stack_size,
        (void *) data,
        instance->config->client_play_session_and_control_task_info.task_prio,
        &instance->server_client_play_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_push_data_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_server_data_task_data_t *data = (espfsp_server_data_task_data_t *) malloc(
        sizeof(espfsp_server_data_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for client push data task");
        return ESP_FAIL;
    }

    data->data_proto = &instance->client_push_data_proto;
    data->server_port = instance->config->client_push_local.data_port;

    xStatus = xTaskCreatePinnedToCore(
        espfsp_server_data_task,
        "espfsp_server_client_push_data_task",
        instance->config->client_push_data_task_info.stack_size,
        (void *) data,
        instance->config->client_push_data_task_info.task_prio,
        &instance->data_recv_task_handle,
        1);

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

    espfsp_server_data_task_data_t *data = (espfsp_server_data_task_data_t *) malloc(
        sizeof(espfsp_server_data_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for client play data task");
        return ESP_FAIL;
    }

    data->data_proto = &instance->client_play_data_proto;
    data->server_port = instance->config->client_play_local.data_port;

    xStatus = xTaskCreate(
        espfsp_server_data_task,
        "espfsp_server_client_play_data_task",
        instance->config->client_play_data_task_info.stack_size,
        (void *) data,
        instance->config->client_play_data_task_info.task_prio,
        &instance->data_send_task_handle);

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

    ret = start_client_push_session_and_control_task(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = start_client_play_session_and_control_task(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = start_client_push_data_task(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return start_client_play_data_task(instance);
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
        ESP_LOGE(TAG, "No free instance to create server");
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

    instance->sender_frame.buf = (char *) malloc(config->frame_config.frame_max_len);
    if (instance->sender_frame.buf == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation for sender frame buffer failed");
        return NULL;
    }

    espfsp_receiver_buffer_config_t receiver_buffer_config = {
        .buffered_fbs = config->buffered_fbs,
        .frame_max_len = config->frame_config.frame_max_len,
    };

    err = espfsp_message_buffer_init(&instance->receiver_buffer, &receiver_buffer_config);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = espfsp_server_comm_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    espfsp_server_session_manager_config_t session_manager_config = {
        .client_push_comm_protos = instance->client_push_comm_proto,
        .client_push_comm_protos_count = CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS,
        .client_play_comm_protos = instance->client_play_comm_proto,
        .client_play_comm_protos_count = CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS,
        .session_id_gen = generate_session_id,
    };

    err = espfsp_session_manager_init(&instance->session_manager, &session_manager_config);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = espfsp_server_data_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = start_tasks(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    heap_caps_check_integrity_all(true); // To debug memory

    return instance;
}

static esp_err_t stop_tasks(espfsp_server_instance_t *instance)
{
    espfsp_data_proto_stop(&instance->client_push_data_proto);
    espfsp_data_proto_stop(&instance->client_play_data_proto);

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS; i++)
    {
        espfsp_comm_proto_stop(&instance->client_push_comm_proto[i]);
    }

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS; i++)
    {
        espfsp_comm_proto_stop(&instance->client_play_comm_proto[i]);
    }

    // Wait for task to stop
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    vTaskDelete(instance->server_client_push_handle);
    vTaskDelete(instance->server_client_play_handle);
    vTaskDelete(instance->data_send_task_handle);
    vTaskDelete(instance->data_recv_task_handle);

    return ESP_OK;
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
        return ret;
    }

    ret = espfsp_server_data_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_session_manager_deinit(&instance->session_manager);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_server_comm_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_message_buffer_deinit(&instance->receiver_buffer);
    if (ret != ESP_OK)
    {
        return ret;
    }

    free(instance->sender_frame.buf);
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
