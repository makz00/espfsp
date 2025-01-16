/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

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

static esp_err_t receive_action_from_sock(
    espfsp_comm_proto_t *comm_proto,
    int sock,
    espfsp_comm_proto_action_t *action,
    espfsp_conn_state_t *conn_state,
    espfsp_comm_proto_tlv_t *tlv_buffer,
    int *received)
{
    esp_err_t ret = ESP_OK;
    *received = 0;

    ret = espfsp_receive_no_block_state(
        sock, (char *) tlv_buffer, sizeof(espfsp_comm_proto_tlv_t), received, conn_state);
    if (ret != ESP_OK)
    {
        *received = 0;
        return ret;
    }

    if ((*received) == 0)
    {
        return ret;
    }

    ESP_LOGI(TAG, "Message received bytes=%d", *received);
    ESP_LOGI(TAG,
             "TLV received type=%d, subtype=%d, len=%d",
             tlv_buffer->type,
             tlv_buffer->subtype,
             tlv_buffer->length);

    action->type = tlv_buffer->type;
    action->subtype = tlv_buffer->subtype;
    action->length = tlv_buffer->length;

    action->data = (uint8_t *) malloc(tlv_buffer->length);
    if (!action->data)
    {
        // Memory allocation did not happen, so we inform that 0 bytes are received
        *received = 0;
        ESP_LOGE(TAG, "Allocation memory for sock received action failed");
        return ESP_FAIL;
    }

    memcpy(action->data, tlv_buffer->value, tlv_buffer->length);
    return ret;
}

static esp_err_t execute_local_action(
    espfsp_comm_proto_t *comm_proto,
    int sock,
    espfsp_comm_proto_action_t *action,
    espfsp_conn_state_t *conn_state,
    espfsp_comm_proto_tlv_t *tlv_buffer)
{
    tlv_buffer->type = action->type;
    tlv_buffer->subtype = action->subtype;
    tlv_buffer->length = action->length;

    if (action->length > sizeof(tlv_buffer->value))
    {
        ESP_LOGE(TAG, "Size of given action is too big to fit in TLV buffer");
        return ESP_FAIL;
    }

    memcpy(tlv_buffer->value, action->data, action->length);

    ESP_LOGI(TAG,
             "TLV to send type=%d, subtype=%d, len=%d",
             tlv_buffer->type,
             tlv_buffer->subtype,
             tlv_buffer->length);

    return espfsp_send_state(sock, (char *) tlv_buffer, sizeof(espfsp_comm_proto_tlv_t), conn_state);
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
        ESP_LOGE(TAG, "Action handler not implemented for subtype: %d", action->subtype);
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
    espfsp_comm_proto_state_t *state, espfsp_comm_proto_state_t next_state, esp_err_t ret_val)
{
    if (ret_val != ESP_OK)
    {
        *state = ESPFSP_COMM_PROTO_STATE_ERROR;
    }
    else
    {
        *state = next_state;
    }
}

static void change_state_base_conn_state(
    espfsp_comm_proto_state_t *state, espfsp_conn_state_t conn_state)
{
    switch (conn_state)
    {
    case ESPFSP_CONN_STATE_GOOD:
        break;

    case ESPFSP_CONN_STATE_CLOSED:
        *state = ESPFSP_COMM_PROTO_STATE_CONN_CLSED;
        break;

    case ESPFSP_CONN_STATE_RESET:
        *state = ESPFSP_COMM_PROTO_STATE_CONN_RESET;
        break;

    case ESPFSP_CONN_STATE_TERMINATED:
        *state = ESPFSP_COMM_PROTO_STATE_CONN_TMNTD;
        break;

    default:
        ESP_LOGE(TAG, "Connection state not handled");
        *state = ESPFSP_COMM_PROTO_STATE_ERROR;
        break;
    }
}

esp_err_t espfsp_comm_proto_run(espfsp_comm_proto_t *comm_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_tlv_t tlv_buffer;
    espfsp_comm_proto_action_t action;
    espfsp_conn_state_t conn_state = ESPFSP_CONN_STATE_GOOD;

    ESP_LOGI(TAG, "Start communication handling");

    // Start from LSISTEN, as actions are planned after received requests
    espfsp_comm_proto_state_t state = ESPFSP_COMM_PROTO_STATE_LISTEN;
    comm_proto->en = 1;

    int64_t reptv_last_called = esp_timer_get_time();
    int64_t reptv_now_called = reptv_last_called;

    while (comm_proto->en)
    {
        switch (state)
        {
        case ESPFSP_COMM_PROTO_STATE_LISTEN:

            taskYIELD();

            int remote_action_received = 0;

            ret = receive_action_from_sock(
                comm_proto, sock, &action, &conn_state, &tlv_buffer, &remote_action_received);

            if (remote_action_received > 0)
            {
                ret = execute_remote_action(comm_proto, &action);
                free(action.data);
            }

            if (ret == ESP_OK && conn_state != ESPFSP_CONN_STATE_GOOD)
            {
                // Connection can also be closed, reset, terminated ..., so it has to be handled here
                change_state_base_conn_state(&state, conn_state);
                break;
            }

            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_ACTION, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_ACTION:

            taskYIELD();

            if (xQueueReceive(comm_proto->reqActionQueue, &action, 0) == pdPASS)
            {
                ret = execute_local_action(comm_proto, sock, &action, &conn_state, &tlv_buffer);
                free(action.data);

                if (ret == ESP_OK && conn_state != ESPFSP_CONN_STATE_GOOD)
                {
                    // Connection can also be closed, reset, terminated ..., so it has to be handled here
                    change_state_base_conn_state(&state, conn_state);
                    break;
                }
            }

            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_REPTIV, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_REPTIV:

            taskYIELD();

            if (comm_proto->config->repetive_callback != NULL)
            {
                reptv_now_called = esp_timer_get_time();

                if (reptv_now_called - reptv_last_called >= comm_proto->config->repetive_callback_freq_us)
                {
                    ret = comm_proto->config->repetive_callback(comm_proto, comm_proto->config->callback_ctx);
                    reptv_last_called = reptv_now_called;
                }
            }

            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_LISTEN, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_RETURN:

            ESP_LOGI(TAG, "Stop communication handling");
            return ESP_OK;

        case ESPFSP_COMM_PROTO_STATE_CONN_CLSED:

            if (comm_proto->config->conn_closed_callback == NULL)
            {
                ESP_LOGE(TAG, "Connection closed state handler not configured");
                change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_ERROR, ESP_FAIL);
                break;
            }

            ret = comm_proto->config->conn_closed_callback(comm_proto, comm_proto->config->callback_ctx);
            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_RETURN, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_CONN_RESET:

            if (comm_proto->config->conn_reset_callback == NULL)
            {
                ESP_LOGE(TAG, "Connection reset state handler not configured");
                change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_ERROR, ESP_FAIL);
                break;
            }

            ret = comm_proto->config->conn_reset_callback(comm_proto, comm_proto->config->callback_ctx);
            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_RETURN, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_CONN_TMNTD:

            if (comm_proto->config->conn_term_callback == NULL)
            {
                ESP_LOGE(TAG, "Connection terminated state handler not configured");
                change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_ERROR, ESP_FAIL);
                break;
            }

            ret = comm_proto->config->conn_term_callback(comm_proto, comm_proto->config->callback_ctx);
            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_RETURN, ret);
            break;

        case ESPFSP_COMM_PROTO_STATE_ERROR:

            ESP_LOGE(TAG, "Communication protocol failed");
            return ESP_FAIL;

        default:

            ESP_LOGE(TAG, "Communication protocol state not handled");
            change_state_base_ret(&state, ESPFSP_COMM_PROTO_STATE_ERROR, ESP_FAIL);
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

    if (data_len > MAX_COMM_PROTO_BUFFER_LEN)
    {
        ESP_LOGE(TAG, "Message too big");
        return ESP_FAIL;
    }

    if (!action.data)
    {
        ESP_LOGE(TAG, "Create action failed");
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
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_INIT,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_init_message_t));
}

esp_err_t espfsp_comm_proto_session_terminate(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_terminate_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_TERMINATE,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_terminate_message_t));
}

esp_err_t espfsp_comm_proto_session_ping(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_ping_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SESSION_PING,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_session_ping_message_t));
}

esp_err_t espfsp_comm_proto_start_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_start_stream_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_START_STREAM,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_start_stream_message_t));
}

esp_err_t espfsp_comm_proto_stop_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_stop_stream_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_STOP_STREAM,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_req_stop_stream_message_t));
}

esp_err_t espfsp_comm_proto_session_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_ack_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_SESSION_ACK,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_session_ack_message_t));
}

esp_err_t espfsp_comm_proto_session_pong(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_pong_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_SESSION_PONG,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_session_pong_message_t));
}

esp_err_t espfsp_comm_proto_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_ack_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_ACK,
        (uint8_t *) msg,
        sizeof(espfsp_comm_proto_resp_ack_message_t));
}

esp_err_t espfsp_comm_proto_cam_set_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_cam_set_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_CAM_SET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_cam_set_params_message_t));
}

esp_err_t espfsp_comm_proto_cam_get_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_cam_get_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_CAM_GET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_cam_get_params_message_t));
}

esp_err_t espfsp_comm_proto_frame_set_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_frame_set_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_FRAME_SET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_frame_set_params_message_t));
}

esp_err_t espfsp_comm_proto_frame_get_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_frame_get_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_FRAME_GET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_frame_get_params_message_t));
}

esp_err_t espfsp_comm_proto_proto_set_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_proto_set_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_PROTO_SET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_proto_set_params_message_t));
}

esp_err_t espfsp_comm_proto_proto_get_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_proto_get_params_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_PROTO_GET_PARAMS,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_proto_get_params_message_t));
}

esp_err_t espfsp_comm_proto_source_set(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_source_set_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SOURCE_SET,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_source_set_message_t));
}

esp_err_t espfsp_comm_proto_source_get(espfsp_comm_proto_t *comm_proto, espfsp_comm_req_source_get_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_REQUEST,
        (uint8_t) ESPFSP_COMM_REQ_SOURCE_GET,
        (uint8_t *) msg,
        sizeof(espfsp_comm_req_source_get_message_t));
}

esp_err_t espfsp_comm_proto_cam_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_resp_cam_params_resp_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_CAM_PARAMS_RESP,
        (uint8_t *) msg,
        sizeof(espfsp_comm_resp_cam_params_resp_message_t));
}

esp_err_t espfsp_comm_proto_frame_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_resp_frame_params_resp_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_FRAME_PARAMS_RESP,
        (uint8_t *) msg,
        sizeof(espfsp_comm_resp_frame_params_resp_message_t));
}

esp_err_t espfsp_comm_proto_proto_params(espfsp_comm_proto_t *comm_proto, espfsp_comm_resp_proto_params_resp_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_PROTO_PARAMS_RESP,
        (uint8_t *) msg,
        sizeof(espfsp_comm_resp_proto_params_resp_message_t));
}

esp_err_t espfsp_comm_proto_sources(espfsp_comm_proto_t *comm_proto, espfsp_comm_resp_sources_resp_message_t *msg)
{
    return insert_action(
        comm_proto,
        ESPFSP_COMM_PROTO_MSG_RESPONSE,
        (uint8_t) ESPFSP_COMM_RESP_SOURCES_RESP,
        (uint8_t *) msg,
        sizeof(espfsp_comm_resp_sources_resp_message_t));
}
