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

#include "espfsp_client_play.h"
#include "espfsp_message_buffer.h"
#include "client_play/espfsp_state_def.h"
#include "client_play/espfsp_comm_proto_conf.h"
#include "client_play/espfsp_data_proto_conf.h"
#include "client_common/espfsp_session_and_control_task.h"
#include "client_common/espfsp_data_task.h"

static const char *TAG = "ESPFSP_CLIENT_PLAY";

static espfsp_client_play_state_t *state_ = NULL;

static esp_err_t start_session_and_control_task(espfsp_client_play_instance_t * instance)
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
    data->local_port = instance->config->local.control_port;
    data->remote_port = instance->config->remote.control_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreate(
        espfsp_client_session_and_control_task,
        "espfsp_client_play_session_and_control_task",
        instance->config->session_and_control_task_info.stack_size,
        (void *) data,
        instance->config->session_and_control_task_info.task_prio,
        &instance->session_and_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start session and control task");
        free(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static espfsp_client_play_instance_t *create_new_client_play(const espfsp_client_play_config_t *config)
{
    espfsp_client_play_instance_t *instance = NULL;

    for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES; i++)
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
        ESP_LOGE(TAG, "No free instance to create client play");
        return NULL;
    }

    instance->config = (espfsp_client_play_config_t *) malloc(sizeof(espfsp_client_play_config_t));
    if (instance->config == NULL)
    {
        ESP_LOGE(TAG, "Config is not initialized");
        return NULL;
    }

    memcpy(instance->config, config, sizeof(espfsp_client_play_config_t));

    instance->session_data.val = false;

    esp_err_t err = ESP_OK;

    espfsp_receiver_buffer_config_t receiver_buffer_config = {
        .buffered_fbs = config->buffered_fbs,
        .frame_max_len = config->frame_config.frame_max_len,
    };

    err = espfsp_message_buffer_init(&instance->receiver_buffer, &receiver_buffer_config);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = espfsp_client_play_comm_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = espfsp_client_play_data_protos_init(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    err = start_session_and_control_task(instance);
    if (err != ESP_OK)
    {
        return NULL;
    }

    return instance;
}

static esp_err_t stop_tasks(espfsp_client_play_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    espfsp_data_proto_stop(&instance->data_proto);
    espfsp_comm_proto_stop(&instance->comm_proto);

    vTaskDelete(instance->data_task_handle);
    vTaskDelete(instance->session_and_control_task_handle);

    return ret;
}

static esp_err_t remove_client_play(espfsp_client_play_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    bool is_given_instance_correct = false;
    for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES; i++)
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

    ret = espfsp_client_play_data_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_client_play_comm_protos_deinit(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_message_buffer_deinit(&instance->receiver_buffer);
    if (ret != ESP_OK)
    {
        return ret;
    }

    free(instance->config);
    instance->used = false;

    return ret;
}

espfsp_client_play_handler_t espfsp_client_play_init(const espfsp_client_play_config_t *config)
{
    if (state_ == NULL)
    {
        state_ = (espfsp_client_play_state_t *) malloc(sizeof(espfsp_client_play_state_t));
        if (state_ == NULL)
        {
            ESP_LOGE(TAG, "State initialization failed");
            return NULL;
        }

        for (int i = 0; i < CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES; i++)
        {
            state_->instances[i].used = false;
        }
    }

    espfsp_client_play_handler_t handler = (espfsp_client_play_handler_t) create_new_client_play(config);
    if (handler == NULL)
    {
        ESP_LOGE(TAG, "Client play creation failed");
    }

    return handler;
}

void espfsp_client_play_deinit(espfsp_client_play_handler_t handler)
{
    if (state_ == NULL)
    {
        ESP_LOGE(TAG, "Client play state is not initialized");
        return;
    }

    esp_err_t err = remove_client_play((espfsp_client_play_instance_t *) handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Client play removal failed");
    }
}

espfsp_fb_t *espfsp_client_play_get_fb(espfsp_client_play_handler_t handler)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;

    espfsp_fb_t *fb = NULL;
    fb = espfsp_message_buffer_get_fb(&instance->receiver_buffer);
    if (fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer get failed");
    }

    return fb;
}

esp_err_t espfsp_client_play_return_fb(espfsp_client_play_handler_t handler, espfsp_fb_t *fb)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;

    esp_err_t ret = ESP_OK;

    ret = espfsp_message_buffer_return_fb(&instance->receiver_buffer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Frame buffer return failed");
    }

    return ret;
}

static esp_err_t start_client_play_data_task(espfsp_client_play_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_client_data_task_data_t *data = (espfsp_client_data_task_data_t *) malloc(
        sizeof(espfsp_client_data_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for client push data task");
        return ESP_FAIL;
    }

    data->data_proto = &instance->data_proto;
    data->comm_proto = &instance->comm_proto;
    data->local_port = instance->config->local.data_port;
    data->remote_port = instance->config->remote.data_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreatePinnedToCore(
        espfsp_client_data_task,
        "espfsp_server_client_push_data_task",
        instance->config->data_task_info.stack_size,
        (void *) data,
        instance->config->data_task_info.task_prio,
        &instance->data_task_handle,
        1);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        free(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_client_play_start_stream(espfsp_client_play_handler_t handler)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_proto_req_start_stream_message_t msg;

    xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY);

    if (!instance->session_data.val)
    {
        xSemaphoreGive(instance->session_data.mutex);
        ESP_LOGE(TAG, "Session with server has not been established");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;
    xSemaphoreGive(instance->session_data.mutex);

    ret = start_client_play_data_task(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return espfsp_comm_proto_start_stream(&instance->comm_proto, &msg);
}

esp_err_t espfsp_client_play_stop_stream(espfsp_client_play_handler_t handler)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_proto_req_stop_stream_message_t msg;

    xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY);

    if (!instance->session_data.val)
    {
        xSemaphoreGive(instance->session_data.mutex);
        ESP_LOGE(TAG, "Session with server has not been established");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;
    xSemaphoreGive(instance->session_data.mutex);

    espfsp_data_proto_stop(&instance->data_proto);

    // Wait a moment

    vTaskDelete(instance->data_task_handle);

    ret = espfsp_message_buffer_clear(&instance->receiver_buffer);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return espfsp_comm_proto_stop_stream(&instance->comm_proto, &msg);
}

esp_err_t espfsp_client_play_reconfigure_frame(espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED");
    return ESP_FAIL;
}

esp_err_t espfsp_client_play_reconfigure_cam(espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED");
    return ESP_FAIL;
}

esp_err_t espfsp_client_play_reconfigure_protocol_params(espfsp_client_play_handler_t handler)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED");
    return ESP_FAIL;
}

esp_err_t espfsp_client_play_get_sources(espfsp_client_play_handler_t handler)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED");
    return ESP_FAIL;
}

esp_err_t espfsp_client_play_set_source(espfsp_client_play_handler_t handler)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED");
    return ESP_FAIL;
}
