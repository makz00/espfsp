/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_sock_op.h"
#include "comm_proto/espfsp_comm_proto.h"

static const char *TAG = "ESPFSP_COMMUNICATION_PROTOCOL";

esp_err_t espfsp_comm_proto_init(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_config_t *config)
{
    esp_err_t ret = ESP_OK;

    comm_proto->config = (espfsp_comm_proto_config_t *) malloc(sizeof(espfsp_comm_proto_config_t));
    if (!comm_proto->config)
    {
        ESP_LOGE(TAG, "Cannot initialize memory for config");
        return ESP_FAIL;
    }

    memcpy(comm_proto->config, config, sizeof(espfsp_comm_proto_config_t));

    comm_proto->state = ESPFSP_COMM_PROTO_STATE_ACTION;

    comm_proto->reqActionQueue = xQueueCreate(comm_proto->buffered_actions, sizeof(espfsp_comm_proto_action_t));
    if (comm_proto->reqActionQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize actions queue");
        free(comm_proto->config);
        return ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_comm_proto_deinit(espfsp_comm_proto_t *comm_proto)
{
    free(comm_proto->config);
    vQueueDelete(comm_proto->reqActionQueue);

    return ESP_OK;
}

static int receive_action_from_sock(espfsp_comm_proto_t *comm_proto, int sock, espfsp_comm_proto_action_t *action)
{
    esp_err_t err = ESP_OK;

    int received = 0;

    err = espfsp_receive_no_block(sock, (void *)&comm_proto->tlv_buffer, sizeof(espfsp_comm_proto_tlv_t), &received);
    if (err != ESP_OK)
    {
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
        return  -1;
    }

    if (received == 0)
    {
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ACTION;
        return 0;
    }

    if (received != comm_proto->tlv_buffer.length + 4)
    {
        ESP_LOGE(TAG, "Error receive TLV message");
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
        return  -1;
    }

    action->type = comm_proto->tlv_buffer.type;
    action->subtype = comm_proto->tlv_buffer.subtype;
    action->length = comm_proto->tlv_buffer.length;

    action->data = (uint8_t *) malloc(comm_proto->tlv_buffer.length);
    if (!action->data)
    {
        ESP_LOGE(TAG, "Allocation memory for sock received action failed");
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
        return  -1;
    }

    memcpy(action->data, comm_proto->tlv_buffer.value, comm_proto->tlv_buffer.length);

    return 1;
}

static esp_err_t execute_action(espfsp_comm_proto_t *comm_proto, int sock, espfsp_comm_proto_action_t *action)
{
    esp_err_t ret = ESP_OK;

    comm_proto->tlv_buffer.type = action->type;
    comm_proto->tlv_buffer.subtype = action->subtype;
    comm_proto->tlv_buffer.length = action->length;

    if (action->length > sizeof(comm_proto->tlv_buffer.value))
    {
        ESP_LOGE(TAG, "Size of given action is too big to fit in TLV buffer");
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
        return ESP_FAIL;
    }

    memcpy(comm_proto->tlv_buffer.value, action->data, action->length);

    ret = espfsp_send(sock, &comm_proto->tlv_buffer, sizeof(espfsp_comm_proto_tlv_t));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Send communicate failed");
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
        return ret;
    }

    comm_proto->state = ESPFSP_COMM_PROTO_STATE_LISTEN;
    return ESP_OK;
}

static esp_err_t execute_handler(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_action_t *action)
{
    esp_err_t ret = ESP_OK;

    switch (action->type)
    {
    case ESPFSP_COMM_PROTO_MSG_REQUEST:

        if (action->subtype >= ESPFSP_COMM_REQ_MAX_NUMBER)
        {
            ESP_LOGE(TAG, "Incorrect request subtype received");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            return ESP_FAIL;
        }

        if (ret = comm_proto->config->req_callbacks[action->subtype] == NULL)
        {
            ESP_LOGE(TAG, "Request handler not implemented");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            return ESP_FAIL;
        }

        ret = comm_proto->config->req_callbacks[action->subtype]((void *) action->data ,comm_proto->config->callback_ctx);
        break;

    case ESPFSP_COMM_PROTO_MSG_RESPONSE:

        if (action->subtype >= ESPFSP_COMM_RESP_MAX_NUMBER)
        {
            ESP_LOGE(TAG, "Incorrect response subtype received");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            return ESP_FAIL;
        }

        if (ret = comm_proto->config->resp_callbacks[action->subtype] == NULL)
        {
            ESP_LOGE(TAG, "Response handler not implemented");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            return ESP_FAIL;
        }

        ret = comm_proto->config->resp_callbacks[action->subtype]((void *) action->data ,comm_proto->config->callback_ctx);
        break;

    default:

        ESP_LOGE(TAG, "Message type is not implemented");
        return ESP_FAIL;

        break;
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Installed handler failed");
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
    }
    else
    {
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ACTION;
    }

    return ret;
}

esp_err_t espfsp_comm_proto_run(espfsp_comm_proto_t *comm_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_action_t action;

    ESP_LOGI(TAG, "Start communication handling");

    while (1)
    {
        switch (comm_proto->state)
        {
        case ESPFSP_COMM_PROTO_STATE_ACTION:

            BaseType_t local_action_status = xQueueReceive(comm_proto->reqActionQueue, &action, 0);
            if (local_action_status == pdPASS)
            {
                ret = execute_action(comm_proto, sock, &action);
                free(action.data);
            }
            break;

        case ESPFSP_COMM_PROTO_STATE_LISTEN:

            int remote_action_status = receive_action_from_sock(comm_proto, sock, &action);
            if (remote_action_status > 0)
            {
                ret = execute_handler(comm_proto, &action);
                free(action.data);
            }
            break;

        case ESPFSP_COMM_PROTO_STATE_TEARDOWN:

            return ret;

        case ESPFSP_COMM_PROTO_STATE_ERROR:

            ESP_LOGE(TAG, "Error ocured");
            if (ret == ESP_OK)
            {
                ESP_LOGE(TAG, "Return value corrected");
                ret = ESP_FAIL;
            }
            return ret;

        default:

            ESP_LOGE(TAG, "Action not handled");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            break;
        }
    }

    ESP_LOGI(TAG, "Stop communication handling");
    return ret;
}

esp_err_t espfsp_comm_proto_stop(espfsp_comm_proto_t *comm_proto)
{
    return ESP_OK;
}

static esp_err_t insert_action(
    espfsp_comm_proto_t *comm_proto,
    espfsp_comm_proto_msg_type_t msg_type,
    uint8_t msg_subtype,
    uint8_t *data,
    uint16_t data_len)
{
    espfsp_comm_proto_action_t action = {
        .type = msg_type,
        .subtype = msg_subtype,
        .length = data_len,
        .data = (uint8_t *) malloc(data_len),
    };

    if (!action.data)
    {
        ESP_LOGE(TAG, "Create action for failed");
        return ESP_FAIL;
    }

    memcpy(action.data, data, data_len);

    if (xQueueSend(comm_proto->reqActionQueue, &action, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot send action to queue");
        free(action.data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_comm_proto_session_init(
    espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_init_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_INIT,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_init_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol session init failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_session_terminate(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_terminate_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_TERMINATE,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_terminate_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol session terminate failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_session_ping(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_ping_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_PING,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_ping_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol ping failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_start_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_start_stream_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_START_STREAM,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_start_stream_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol stream start failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_stop_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_stop_stream_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_STOP_STREAM,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_stop_stream_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol stream stop failed");
    }

    return ret;
}
