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
#include "server/espfsp_data_task.h"

static const char *TAG = "SERVER_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t req_session_init_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_init_message_t *msg = (espfsp_comm_proto_req_session_init_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    espfsp_comm_proto_resp_session_ack_message_t resp;
    espfsp_server_session_data_t **session_data = NULL;

    switch (msg->client_type)
    {
    case ESPFSP_COMM_REQ_CLIENT_PUSH:

        resp.session_id = instance->client_push_next_session_id++;
        ret = handle_session_init_for_client_push(comm_proto, msg, instance);
        session_data = &instance->client_push_session_data[0];
        break;

    case ESPFSP_COMM_REQ_CLIENT_PLAY:

        resp.session_id = instance->client_play_next_session_id++;
        ret = handle_session_init_for_client_play(comm_proto, msg, instance);
        session_data = &instance->client_play_session_data[0];
        break;
    }

    *session_data = (espfsp_server_session_data_t *) malloc(sizeof(espfsp_server_session_data_t));
    if (*session_data == NULL)
    {
        return ESP_FAIL;
    }
    (*session_data)->session_id = resp.session_id;

    return espfsp_comm_proto_session_ack(comm_proto, &resp);
}

esp_err_t req_session_terminate_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    espfsp_server_session_data_t *session_data = NULL;

    if (instance->client_push_session_data[0] != NULL && instance->client_push_session_data[0]->session_id == msg->session_id)
    {
        session_data = instance->client_push_session_data[0];
    }

    if (instance->client_play_session_data[0] != NULL && instance->client_play_session_data[0]->session_id == msg->session_id)
    {
        session_data = instance->client_play_session_data[0];
    }

    free(session_data);
    return ret;
}

static esp_err_t start_client_push_data_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_data_task_data_t *data = (espfsp_data_task_data_t *) malloc(
        sizeof(espfsp_data_task_data_t));

    if (data == NULL)
    {
        return ESP_FAIL;
    }

    data->data_proto = &instance->client_push_data_proto;
    data->comm_proto = &instance->client_push_comm_proto;
    data->port = instance->config->client_push_local.data_port;

    xStatus = xTaskCreatePinnedToCore(
        espfsp_server_data_task,
        "espfsp_server_client_push_data_task",
        instance->config->client_push_data_task_info.stack_size,
        (void *) data,
        instance->config->client_push_data_task_info.task_prio,
        &instance->client_push_handlers[0].data_task_handle,
        1);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        free(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_client_play_data_task(espfsp_server_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_data_task_data_t *data = (espfsp_data_task_data_t *) malloc(
        sizeof(espfsp_data_task_data_t));

    if (data == NULL)
    {
        return ESP_FAIL;
    }

    data->data_proto = &instance->client_play_data_proto;
    data->comm_proto = &instance->client_play_comm_proto;
    data->port = instance->config->client_play_local.data_port;

    xStatus = xTaskCreate(
        espfsp_server_data_task,
        "espfsp_server_client_play_data_task",
        instance->config->client_play_data_task_info.stack_size,
        (void *) data,
        instance->config->client_play_data_task_info.task_prio,
        &instance->client_play_handlers[0].data_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        free(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t start_stream_tasks(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    ret = start_client_push_data_task(instance);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = start_client_play_data_task(instance);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

esp_err_t req_start_stream_server_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;

    if (instance->client_push_session_data[0] = NULL)
    {
        return ESP_OK;
    }

    espfsp_comm_proto_req_start_stream_message_t msg = {
        .session_id = instance->client_push_session_data[0]->session_id,
    };

    ret = espfsp_comm_proto_start_stream(&instance->client_push_comm_proto, &msg);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return start_stream_tasks(instance);
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

    if (instance->client_push_session_data[0] = NULL)
    {
        return ESP_OK;
    }

    espfsp_comm_proto_req_stop_stream_message_t msg = {
        .session_id = instance->client_push_session_data[0]->session_id,
    };

    ret = espfsp_comm_proto_stop_stream(&instance->client_push_comm_proto, &msg);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = stop_stream_tasks(instance);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // It is allowed to call this method as CLIENT_PUSH and CLIENT_PLAY data tasks should be stopped
    return espfsp_message_buffer_clear(&instance->receiver_buffer);
}
