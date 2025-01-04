/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"

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

    comm_proto->reqActionQueue = xQueueCreate(comm_proto->config->buffered_actions, sizeof(espfsp_comm_proto_action_t));
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

    espfsp_comm_proto_action_t action;
    while (xQueueReceive(comm_proto->reqActionQueue, &action, 0) == pdPASS)
    {
        free(action.data);
    }
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
        return  -1;
    }

    if (received == 0)
    {
        return 0;
    }

    if (received != comm_proto->tlv_buffer.length + 4)
    {
        ESP_LOGE(TAG, "Receive bytes does not match length in TLV message");
        return  -1;
    }

    action->type = comm_proto->tlv_buffer.type;
    action->subtype = comm_proto->tlv_buffer.subtype;
    action->length = comm_proto->tlv_buffer.length;

    action->data = (uint8_t *) malloc(comm_proto->tlv_buffer.length);
    if (!action->data)
    {
        ESP_LOGE(TAG, "Allocation memory for sock received action failed");
        return  -1;
    }

    memcpy(action->data, comm_proto->tlv_buffer.value, comm_proto->tlv_buffer.length);
    return 1;
}

static esp_err_t execute_local_action(espfsp_comm_proto_t *comm_proto, int sock, espfsp_comm_proto_action_t *action)
{
    esp_err_t ret = ESP_OK;

    comm_proto->tlv_buffer.type = action->type;
    comm_proto->tlv_buffer.subtype = action->subtype;
    comm_proto->tlv_buffer.length = action->length;

    if (action->length > sizeof(comm_proto->tlv_buffer.value))
    {
        ESP_LOGE(TAG, "Size of given action is too big to fit in TLV buffer");
        return ESP_FAIL;
    }

    memcpy(comm_proto->tlv_buffer.value, action->data, action->length);

    ret = espfsp_send(sock, &comm_proto->tlv_buffer, sizeof(espfsp_comm_proto_tlv_t));
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ret;
}

static esp_err_t check_and_execute_action(
    espfsp_comm_proto_t *comm_proto,
    espfsp_comm_proto_action_t *action,
    __espfsp_comm_proto_msg_cb *action_callbacks,
    uint8_t action_max_index)
{
    if (action->subtype >= action_max_index)
    {
        ESP_LOGE(TAG, "Incorrect action subtype received");
        return ESP_FAIL;
    }

    if (action_callbacks[action->subtype] == NULL)
    {
        ESP_LOGE(TAG, "Action handler not implemented");
        return ESP_FAIL;
    }

    return action_callbacks[action->subtype](comm_proto, (void *) action->data, comm_proto->config->callback_ctx);
}

static esp_err_t execute_remote_action(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_action_t *action)
{
    esp_err_t ret = ESP_OK;

    switch (action->type)
    {
    case ESPFSP_COMM_PROTO_MSG_REQUEST:

        ret = check_and_execute_action(
            comm_proto, action, comm_proto->config->req_callbacks, ESPFSP_COMM_REQ_MAX_NUMBER);
        break;

    case ESPFSP_COMM_PROTO_MSG_RESPONSE:

        ret = check_and_execute_action(
            comm_proto, action, comm_proto->config->resp_callbacks, ESPFSP_COMM_RESP_MAX_NUMBER);
        break;

    default:

        ESP_LOGE(TAG, "Action type is not implemented");
        return ESP_FAIL;
    }

    return ret;
}

static void change_state_base_ret(
    espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_state_t next_state, esp_err_t ret_val)
{
    if (ret_val != ESP_OK)
    {
        comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
    }
    else
    {
        comm_proto->state = next_state;
    }
}

esp_err_t espfsp_comm_proto_run(espfsp_comm_proto_t *comm_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_action_t action;

    ESP_LOGI(TAG, "Start communication handling");

    // Start from LSISTEN, as server actions are planned after received requests
    comm_proto->state = ESPFSP_COMM_PROTO_STATE_LISTEN;
    comm_proto->en = 1;

    int64_t reptv_last_called, reptv_now_called = esp_timer_get_time();

    while (comm_proto->en)
    {
        switch (comm_proto->state)
        {
        case ESPFSP_COMM_PROTO_STATE_LISTEN:

            int remote_action_status = receive_action_from_sock(comm_proto, sock, &action);
            if (remote_action_status > 0)
            {
                ret = execute_remote_action(comm_proto, &action);
                free(action.data);
            }
            else if (remote_action_status < 0)
            {
                ret = ESP_FAIL;
            }

            change_state_base_ret(comm_proto, ESPFSP_COMM_PROTO_STATE_ACTION, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_ACTION:

            BaseType_t local_action_status = xQueueReceive(comm_proto->reqActionQueue, &action, 0);
            if (local_action_status == pdPASS)
            {
                ret = execute_local_action(comm_proto, sock, &action);
                free(action.data);
            }

            change_state_base_ret(comm_proto, ESPFSP_COMM_PROTO_STATE_REPTIV, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_REPTIV:

            if (comm_proto->config->repetive_callback != NULL)
            {
                reptv_now_called = esp_timer_get_time();

                if (reptv_now_called - reptv_last_called >= comm_proto->config->repetive_callback_freq_us)
                {
                    ret = comm_proto->config->repetive_callback(comm_proto, comm_proto->config->callback_ctx);
                    reptv_last_called = reptv_now_called;
                }
            }

            change_state_base_ret(comm_proto, ESPFSP_COMM_PROTO_STATE_LISTEN, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_ERROR:

            ESP_LOGE(TAG, "Communication protocol failed");
            return ESP_FAIL;

        default:

            ESP_LOGE(TAG, "Communication protocol state not handled");
            comm_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
            break;
        }
    }

    return ESP_OK;
}

esp_err_t espfsp_comm_proto_stop(espfsp_comm_proto_t *comm_proto)
{
    // Safe asynchronious write as the other tash, after start, only reads this variable
    comm_proto->en = 0;
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

esp_err_t espfsp_comm_proto_session_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_ack_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_SESSION_ACK,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_session_ack_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol session ack failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_session_pong(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_pong_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_SESSION_PONG,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_session_pong_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol session pong failed");
    }

    return ret;
}

esp_err_t espfsp_comm_proto_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_ack_message_t *msg)
{
    esp_err_t ret = insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_ACK,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_ack_message_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol ack failed");
    }

    return ret;
}
