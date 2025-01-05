/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "comm_proto/espfsp_comm_proto.h"
#include "client_push/espfsp_state_def.h"
#include "client_push/espfsp_comm_proto_handlers.h"
#include "client_common/espfsp_data_task.h"

static const char *TAG = "CLIENT_PUSH_COMMUNICATION_PROTOCOL_HANDLERS";

esp_err_t espfsp_client_push_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_session_terminate_message_t *msg = (espfsp_comm_proto_req_session_terminate_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (instance->session_data.val && instance->session_data.session_id == msg->session_id)
    {
        instance->session_data.val = false;
    }

    return ret;
}

static esp_err_t start_client_push_data_task(espfsp_client_push_instance_t * instance)
{
    BaseType_t xStatus;

    espfsp_client_data_task_data_t *data = (espfsp_client_data_task_data_t *) malloc(
        sizeof(espfsp_client_data_task_data_t));

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for data task");
        return ESP_FAIL;
    }

    data->data_proto = &instance->data_proto;
    data->comm_proto = &instance->comm_proto;
    data->local_port = instance->config->local.data_port;
    data->remote_port = instance->config->remote.data_port;
    data->remote_addr.addr = instance->config->remote_addr.addr;

    xStatus = xTaskCreatePinnedToCore(
        espfsp_client_data_task,
        "espfsp_client_push_data_task",
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

esp_err_t espfsp_client_push_req_start_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_start_stream_message_t *msg = (espfsp_comm_proto_req_start_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.val || instance->session_data.session_id != msg->session_id)
    {
        ESP_LOGE(TAG, "No session created OR session id does not match");
        return ESP_OK;
    }

    ret = instance->config->cb.start_cam(&instance->config->cam_config, &instance->config->frame_config);
    if (ret != ESP_OK)
    {
        espfsp_comm_proto_req_session_terminate_message_t msg = {
            .session_id = msg.session_id,
        };

        return espfsp_comm_proto_session_terminate(comm_proto, &msg);
    }

    return start_client_push_data_task(instance);
}

esp_err_t espfsp_client_push_req_stop_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_req_stop_stream_message_t *msg = (espfsp_comm_proto_req_stop_stream_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (!instance->session_data.val || instance->session_data.session_id != msg->session_id)
    {
        ESP_LOGE(TAG, "No session created OR session id does not match");
        return ESP_OK;
    }

    espfsp_data_proto_stop(&instance->data_proto);

    // Wait a moment

    vTaskDelete(instance->data_task_handle);

    ret = instance->config->cb.stop_cam();
    if (ret != ESP_OK)
    {
        espfsp_comm_proto_req_session_terminate_message_t msg = {
            .session_id = msg.session_id,
        };

        return espfsp_comm_proto_session_terminate(comm_proto, &msg);
    }

    return ret;
}

esp_err_t espfsp_client_push_resp_session_ack_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_resp_session_ack_message_t *msg = (espfsp_comm_proto_resp_session_ack_message_t *) msg_content;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;

    if (instance->session_data.val)
    {
        ESP_LOGE(TAG, "Cannot handle new session ack. There is already one session");
        return ESP_FAIL;
    }

    instance->session_data.val = true;
    instance->session_data.session_id = msg->session_id;

    return ret;
}
