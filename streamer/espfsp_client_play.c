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

#include "espfsp_params_map.h"
#include "espfsp_client_play.h"
#include "espfsp_message_buffer.h"
#include "client_play/espfsp_state_def.h"
#include "client_play/espfsp_comm_proto_conf.h"
#include "client_play/espfsp_data_proto_conf.h"
#include "client_common/espfsp_session_and_control_task.h"
#include "client_common/espfsp_data_task.h"

#define GET_REQUESTS_WAIT_TICK_MS 10

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
    data->client_type = ESPFSP_COMM_REQ_CLIENT_PLAY;
    data->local_port = instance->config->local.control_port;
    data->remote_port = instance->config->remote.control_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreate(
        espfsp_client_session_and_control_task,
        "session_and_control_task",
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

static esp_err_t start_data_task(espfsp_client_play_instance_t * instance)
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
    data->local_port = instance->config->local.data_port;
    data->remote_port = instance->config->remote.data_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreate(
        espfsp_client_data_task,
        "data_task",
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

    instance->session_data.session_id = -1;
    instance->session_data.active = false;
    instance->session_data.stream_started = false;

    instance->session_data.mutex = NULL;
    instance->session_data.mutex = xSemaphoreCreateBinary();
    if (instance->session_data.mutex == NULL)
    {
        ESP_LOGE(TAG, "Cannot init semaphore");
        return NULL;
    }

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give after init semaphore");
        return NULL;
    }

    esp_err_t err = ESP_OK;

    instance->get_sources_data_queue = NULL;
    instance->get_sources_data_queue = xQueueCreate(1, sizeof(espfsp_get_sources_data_t));
    if (instance->get_sources_data_queue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize queue for get sources request");
        return NULL;
    }

    instance->get_frame_config_data_queue = NULL;
    instance->get_frame_config_data_queue = xQueueCreate(frame_param_map_size, sizeof(espfsp_get_param_data_t));
    if (instance->get_frame_config_data_queue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize queue for get frame config");
        return NULL;
    }

    instance->get_cam_config_data_queue = NULL;
    instance->get_cam_config_data_queue = xQueueCreate(cam_param_map_size, sizeof(espfsp_get_param_data_t));
    if (instance->get_cam_config_data_queue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize queue for get camera config");
        return NULL;
    }

    espfsp_receiver_buffer_config_t receiver_buffer_config = {
        .buffered_fbs = config->frame_config.buffered_fbs,
        .frame_max_len = config->frame_config.frame_max_len,
        .fb_in_buffer_before_get = config->frame_config.fb_in_buffer_before_get,
        .fps = config->frame_config.fps,
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

    err = start_data_task(instance);
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

    // Wait for task to stop
    vTaskDelay(1000 / portTICK_PERIOD_MS);

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

    vQueueDelete(instance->get_sources_data_queue);
    vQueueDelete(instance->get_frame_config_data_queue);
    vQueueDelete(instance->get_cam_config_data_queue);
    vSemaphoreDelete(instance->session_data.mutex);

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

espfsp_fb_t *espfsp_client_play_get_fb(espfsp_client_play_handler_t handler, uint32_t timeout_ms)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return NULL;
    }

    if (!instance->session_data.active || !instance->session_data.stream_started)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return NULL;
        }

        return NULL;
    }

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return NULL;
    }

    return espfsp_message_buffer_get_fb(&instance->receiver_buffer, timeout_ms);
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

esp_err_t espfsp_client_play_start_stream(espfsp_client_play_handler_t handler)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_proto_req_start_stream_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active || instance->session_data.stream_started)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }

        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    instance->session_data.stream_started = true;
    msg.session_id = instance->session_data.session_id;

    if (instance->config->frame_config.buffered_fbs != instance->receiver_buffer.config->buffered_fbs ||
        instance->config->frame_config.fb_in_buffer_before_get != instance->receiver_buffer.config->fb_in_buffer_before_get ||
        instance->config->frame_config.frame_max_len != instance->receiver_buffer.config->frame_max_len ||
        instance->config->frame_config.fps != instance->receiver_buffer.config->fps)
    {
        ret = espfsp_message_buffer_deinit(&instance->receiver_buffer);
        if (ret == ESP_OK)
        {
            espfsp_receiver_buffer_config_t new_config = {
                .buffered_fbs = instance->config->frame_config.buffered_fbs,
                .fb_in_buffer_before_get = instance->config->frame_config.fb_in_buffer_before_get,
                .frame_max_len = instance->config->frame_config.frame_max_len,
                .fps = instance->config->frame_config.fps,
            };

            ret = espfsp_message_buffer_init(&instance->receiver_buffer, &new_config);
        }
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Receiver buffer reconfiguration failed");
        }
    }

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    if (ret == ESP_OK)
    {
        ret = espfsp_data_proto_start(&instance->data_proto);
    }
    if (ret == ESP_OK)
    {
        ret = espfsp_comm_proto_start_stream(&instance->comm_proto, &msg);
    }

    return ret;
}

esp_err_t espfsp_client_play_stop_stream(espfsp_client_play_handler_t handler)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_proto_req_stop_stream_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    instance->session_data.stream_started = false;
    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    ret = espfsp_data_proto_stop(&instance->data_proto);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return espfsp_comm_proto_stop_stream(&instance->comm_proto, &msg);
}

esp_err_t espfsp_client_play_reconfigure_frame(
    espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_frame_set_params_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    for (int i = 0; i < frame_param_map_size; i++)
    {
        uint16_t param_id = frame_param_map[i].param_id;
        msg.param_id = param_id;

        if (ret == ESP_OK)
        {
            ret = espfsp_params_map_get_frame_config_param_val(frame_config, param_id, &msg.value);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_comm_proto_frame_set_params(&instance->comm_proto, &msg);
        }
    }

    if (ret == ESP_OK)
    {
        memcpy(&instance->config->frame_config, frame_config, sizeof(espfsp_frame_config_t));
    }

    return ret;
}

esp_err_t espfsp_client_play_get_frame(
    espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_frame_get_params_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    for (int i = 0; i < frame_param_map_size; i++)
    {
        msg.param_id = frame_param_map[i].param_id;

        if (ret == ESP_OK)
        {
            ret = espfsp_comm_proto_frame_get_params(&instance->comm_proto, &msg);
        }
    }

    int params_to_receive = frame_param_map_size;
    espfsp_get_param_data_t param_data;

    if (ret == ESP_OK)
    {
        while (timeout_ms > 0 && params_to_receive > 0)
        {
            if (xQueueReceive(instance->get_frame_config_data_queue, &param_data, 0) == pdTRUE)
            {
                espfsp_params_map_set_frame_config(frame_config, param_data.param_id, param_data.param_value);
                params_to_receive--;
            }
            else
            {
                vTaskDelay(GET_REQUESTS_WAIT_TICK_MS / portTICK_PERIOD_MS);
                timeout_ms -= GET_REQUESTS_WAIT_TICK_MS;
            }
        }
    }

    if (params_to_receive > 0)
    {
        ESP_LOGI(TAG, "Cannot receive all frame params. Try increase timeout");
    }

    return ret;
}

esp_err_t espfsp_client_play_reconfigure_cam(
    espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_cam_set_params_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    for (int i = 0; i < cam_param_map_size; i++)
    {
        uint16_t param_id = cam_param_map[i].param_id;
        msg.param_id = param_id;

        if (ret == ESP_OK)
        {
            ret = espfsp_params_map_get_cam_config_param_val(cam_config, param_id, &msg.value);
        }
        if (ret == ESP_OK)
        {
            ret = espfsp_comm_proto_cam_set_params(&instance->comm_proto, &msg);
        }
    }

    return ret;
}

esp_err_t espfsp_client_play_get_cam(
    espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_cam_get_params_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    for (int i = 0; i < cam_param_map_size; i++)
    {
        msg.param_id = cam_param_map[i].param_id;

        if (ret == ESP_OK)
        {
            ret = espfsp_comm_proto_cam_get_params(&instance->comm_proto, &msg);
        }
    }

    int params_to_receive = cam_param_map_size;
    espfsp_get_param_data_t param_data;

    if (ret == ESP_OK)
    {
        while (timeout_ms > 0 && params_to_receive > 0)
        {
            if (xQueueReceive(instance->get_cam_config_data_queue, &param_data, 0) == pdTRUE)
            {
                espfsp_params_map_set_cam_config(cam_config, param_data.param_id, param_data.param_value);
                params_to_receive--;
            }
            else
            {
                vTaskDelay(GET_REQUESTS_WAIT_TICK_MS / portTICK_PERIOD_MS);
                timeout_ms -= GET_REQUESTS_WAIT_TICK_MS;
            }
        }
    }

    if (params_to_receive > 0)
    {
        ESP_LOGI(TAG, "Cannot receive all camera params. Try increase timeout");
    }

    return ret;
}

esp_err_t espfsp_client_play_get_sources_timeout(
    espfsp_client_play_handler_t handler,
    char sources_names_buf[SOURCE_NAMES_MAX][SOURCE_NAME_LEN_MAX],
    int *sources_names_len,
    uint32_t timeout_ms)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_source_get_message_t msg;
    espfsp_get_sources_data_t sources_data;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    esp_err_t ret = espfsp_comm_proto_source_get(&instance->comm_proto, &msg);
    if (ret == ESP_OK)
    {
        while (timeout_ms > 0)
        {
            if (xQueuePeek(instance->get_sources_data_queue, &sources_data, 0) == pdTRUE)
            {
                break;
            }
            else
            {
                vTaskDelay(GET_REQUESTS_WAIT_TICK_MS / portTICK_PERIOD_MS);
                timeout_ms -= GET_REQUESTS_WAIT_TICK_MS;
            }
        }

        if (xQueueReceive(instance->get_sources_data_queue, &sources_data, 0) != pdTRUE)
        {
            ESP_LOGI(TAG, "Cannot receive producent value from queue");
            *sources_names_len = 0;

            return ESP_OK;
        }
    }
    if (ret == ESP_OK)
    {
        int sources_count_to_copy = *sources_names_len >= sources_data.sources_names_len ? sources_data.sources_names_len : *sources_names_len;

        for (int i = 0; i < sources_count_to_copy; i++)
        {
            memcpy(sources_names_buf[i], sources_data.sources_names_buf[i], SOURCE_NAME_LEN_MAX);
        }

        *sources_names_len = sources_count_to_copy;
    }

    return ret;
}

esp_err_t espfsp_client_play_set_source(
    espfsp_client_play_handler_t handler, const char source_name[SOURCE_NAME_LEN_MAX])
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) handler;
    espfsp_comm_req_source_set_message_t msg;

    if (xSemaphoreTake(instance->session_data.mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    if (!instance->session_data.active && instance->session_data.stream_started)
    {
        if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Cannot give semaphore");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG, "Session state is not correct for this request");
        return ESP_FAIL;
    }

    msg.session_id = instance->session_data.session_id;

    if (xSemaphoreGive(instance->session_data.mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    memcpy(msg.source_name, source_name, sizeof(msg.source_name));

    return espfsp_comm_proto_source_set(&instance->comm_proto, &msg);
}
