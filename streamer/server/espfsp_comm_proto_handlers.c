/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "comm_proto/espfsp_comm_proto.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_comm_proto_handlers.h"
#include "server/espfsp_client_play_data_task.h"
#include "server/espfsp_client_push_data_task.h"

static const char *TAG = "SERVER_COMMUNICATION_PROTOCOL_HANDLERS";

static esp_err_t init_session_data(espfsp_server_instance_t *instance, int session_id, espfsp_comm_proto_req_client_type_t client_type)
{
    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_CONNECTIONS; i++)
    {
        if (instance->session_data[i] != NULL)
        {
            instance->session_data[i] = (espfsp_server_session_data_t *) malloc(sizeof(espfsp_server_session_data_t));
            instance->session_data[i]->session_id = session_id;
            instance->session_data[i]->client_type = client_type;
            ESP_LOGI(TAG, "Registered client with session id=%d", session_id);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "No resources to initiate new session for client");
    return ESP_FAIL;
}

static esp_err_t  deinit_session_data(espfsp_server_instance_t *instance, int session_id)
{
    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_CONNECTIONS; i++)
    {
        if (instance->session_data[i] != NULL && instance->session_data[i]->session_id == session_id)
        {
            free(instance->session_data[i]);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "No resources to deinitiate session for client");
    return ESP_FAIL;
}

static esp_err_t find_client_type(espfsp_server_instance_t *instance, int session_id, espfsp_comm_proto_req_client_type_t *client_type)
{
    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_CONNECTIONS; i++)
    {
        if (instance->session_data[i] != NULL && instance->session_data[i]->session_id == session_id)
        {
            *client_type = instance->session_data[i]->client_type;
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Cannot find client with session id=%d", session_id);
    return ESP_FAIL;
}

static esp_err_t get_active_client_push_session_id(espfsp_server_instance_t *instance, int *session_id)
{
    for (int i = 0; i < CONFIG_ESPFSP_SERVER_MAX_CONNECTIONS; i++)
    {
        if (instance->session_data[i] != NULL && instance->session_data[i]->client_type == ESPFSP_COMM_REQ_CLIENT_PUSH)
        {
            *session_id = instance->session_data[i]->session_id;
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Cannot find any CLIENT_PUSH");
    return ESP_FAIL;
}


static esp_err_t handle_session_init_for_client_push(
    espfsp_comm_proto_t *comm_proto,
    espfsp_comm_proto_req_session_init_message_t *msg,
    espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    espfsp_comm_proto_resp_session_ack_message_t resp = {
        .session_id = instance->client_push_next_session_id++,
    };

    ret = init_session_data(instance, resp.session_id, ESPFSP_COMM_REQ_CLIENT_PUSH);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initiate session data for CLIENT_PUSH");
        return ret;
    }

    ret = espfsp_comm_proto_session_ack(comm_proto, &resp);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initiate session for CLIENT_PUSH");
    }

    return ret;
}

static esp_err_t handle_session_init_for_client_play(
    espfsp_comm_proto_t *comm_proto,
    espfsp_comm_proto_req_session_init_message_t *msg,
    espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    espfsp_comm_proto_resp_session_ack_message_t resp = {
        .session_id = instance->client_play_next_session_id++,
    };

    ret = init_session_data(instance, resp.session_id, ESPFSP_COMM_REQ_CLIENT_PLAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initiate session data for CLIENT_PLAY");
        return ret;
    }

    ret = espfsp_comm_proto_session_ack(comm_proto, &resp);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initiate session for CLIENT_PLAY");
    }

    return ret;
}

esp_err_t req_session_init_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_init_message_t *msg = (espfsp_comm_proto_req_session_init_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    switch (msg->client_type)
    {
    case ESPFSP_COMM_REQ_CLIENT_PUSH:

        ret = handle_session_init_for_client_push(comm_proto, msg, instance);
        break;

    case ESPFSP_COMM_REQ_CLIENT_PLAY:

        ret = handle_session_init_for_client_play(comm_proto, msg, instance);
        break;

    default:

        ESP_LOGE(TAG, "Unsupported type of client");
        return ESP_FAIL;
    }

    return ret;
}

esp_err_t req_session_terminate_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    ret = deinit_session_data(instance, msg->session_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Deinitiation session failed");
    }

    return ret;
}

static esp_err_t request_stream_start_from_client_push(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    int client_push_session_id = 0;
    ret = get_active_client_push_session_id(instance, &client_push_session_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Get active client push session id failed");
        return ret;
    }

    espfsp_comm_proto_req_start_stream_message_t msg = {
        .session_id = client_push_session_id,
    };

    ret = espfsp_comm_proto_start_stream(&instance->client_push_comm_proto, &msg);
    if (ret != ESP_OK)
    {
        ESPL_LOGE(TAG, "Start stream send action to CLIENT_PUSH failed");
        return ret;
    }

    return ret;
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

static esp_err_t start_stream_tasks(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    ret = start_client_push_data_task(instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data client push task has not been created successfully");
        return ret;
    }

    ret = start_client_play_data_task(instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data client play task has not been created successfully");
        return ret;
    }

    return ret;
}

esp_err_t req_start_stream_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    espfsp_comm_proto_req_client_type_t client_type;
    ret = find_client_type(instance, msg->session_id, &client_type);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "No client with given session ID found");
        return ret;
    }

    switch (client_type)
    {
    case ESPFSP_COMM_REQ_CLIENT_PUSH:

        ESP_LOGE(TAG, "Request stream start received from CLIENT_PUSH. This should not happen");
        break;

    case ESPFSP_COMM_REQ_CLIENT_PLAY:

        ret = request_stream_start_from_client_push(instance);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Request stream start from CLIENT_PUSH failed");
            return ret;
        }

        ret = start_stream_tasks(instance);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Start stream tasks failed");
            return ret;
        }

        break;

    default:

        ESP_LOGE(TAG, "Unsupported type of client");
        return ESP_FAIL;
    }

    return ret;
}

static esp_err_t stop_stream_tasks(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    vTaskDelete(instance->client_push_handlers[0].data_task_handle);
    vTaskDelete(instance->client_play_handlers[0].data_task_handle);

    return ret;
}

esp_err_t req_stop_stream_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    espfsp_comm_proto_req_client_type_t client_type;
    ret = find_client_type(instance, msg->session_id, &client_type);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "No client with given session ID found");
        return ret;
    }

    switch (client_type)
    {
    case ESPFSP_COMM_REQ_CLIENT_PUSH:

        ESP_LOGE(TAG, "Request stream start received from CLIENT_PUSH. This should not happen");
        break;

    case ESPFSP_COMM_REQ_CLIENT_PLAY:

        ret = request_stream_stop_from_client_push(instance);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Request stream stop from CLIENT_PUSH failed");
            return ret;
        }

        ret = stop_stream_tasks(instance);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Stop stream tasks failed");
            return ret;
        }

        // It is allowed to call this method as CLIENT_PUSH and CLIENT_PLAY data tasks should be stopped
        ret = espfsp_message_buffer_clear(&instance->receiver_buffer);
        {
            ESP_LOGE(TAG, "Clear receive buffer failed");
            return ret;
        }

        break;

    default:

        ESP_LOGE(TAG, "Unsupported type of client");
        return ESP_FAIL;
    }

    return ret;
}
