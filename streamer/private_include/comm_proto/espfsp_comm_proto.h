/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stddef.h>

#include "espfsp_comm_proto_req.h"
#include "espfsp_comm_proto_resp.h"

#define MAX_COMM_PROTO_BUFFER_LEN 256

typedef enum {
    ESPFSP_COMM_PROTO_STATE_ACTION,
    ESPFSP_COMM_PROTO_STATE_LISTEN,
    ESPFSP_COMM_PROTO_STATE_REPTIV,
    ESPFSP_COMM_PROTO_STATE_RETURN,
    ESPFSP_COMM_PROTO_STATE_CONN_CLSED,
    ESPFSP_COMM_PROTO_STATE_CONN_RESET,
    ESPFSP_COMM_PROTO_STATE_CONN_TMNTD,
    ESPFSP_COMM_PROTO_STATE_ERROR,
} espfsp_comm_proto_state_t;

typedef enum {
    ESPFSP_COMM_PROTO_MSG_REQUEST = 0x01,
    ESPFSP_COMM_PROTO_MSG_RESPONSE = 0x02,
} espfsp_comm_proto_msg_type_t;

typedef struct {
    espfsp_comm_proto_msg_type_t type;
    uint8_t subtype;
    uint16_t length;
    uint8_t value[MAX_COMM_PROTO_BUFFER_LEN];
} espfsp_comm_proto_tlv_t;

typedef struct {
    espfsp_comm_proto_msg_type_t type;
    uint8_t subtype;
    uint16_t length;
    uint8_t *data;
} espfsp_comm_proto_action_t;

typedef struct espfsp_comm_proto_t espfsp_comm_proto_t;

// msg_content parameter is structure representing message type
// This kind of structures definitions are in *_resp.h and in *_req.h
// Contex is defined by user of this interface, passed in config as callback_ctx
typedef esp_err_t (*__espfsp_comm_proto_msg_cb)(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);

typedef esp_err_t (*__espfsp_comm_proto_cb)(espfsp_comm_proto_t *comm_proto, void *ctx);

typedef struct {
    __espfsp_comm_proto_msg_cb req_callbacks[ESPFSP_COMM_REQ_MAX_NUMBER];
    __espfsp_comm_proto_msg_cb resp_callbacks[ESPFSP_COMM_RESP_MAX_NUMBER];
    __espfsp_comm_proto_cb repetive_callback;
    __espfsp_comm_proto_cb conn_closed_callback;
    __espfsp_comm_proto_cb conn_reset_callback;
    __espfsp_comm_proto_cb conn_term_callback;
    uint64_t repetive_callback_freq_us;
    void *callback_ctx;
    int buffered_actions;
} espfsp_comm_proto_config_t;

struct espfsp_comm_proto_t {
    espfsp_comm_proto_config_t *config;
    QueueHandle_t reqActionQueue;
    uint8_t en;
};

esp_err_t espfsp_comm_proto_init(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_config_t *config);
esp_err_t espfsp_comm_proto_deinit(espfsp_comm_proto_t *comm_proto);

esp_err_t espfsp_comm_proto_run(espfsp_comm_proto_t *comm_proto, int sock);
esp_err_t espfsp_comm_proto_stop(espfsp_comm_proto_t *comm_proto);

// Actions for requests --- BEGIN
esp_err_t espfsp_comm_proto_session_init(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_init_message_t *msg);
esp_err_t espfsp_comm_proto_session_terminate(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_terminate_message_t *msg);
esp_err_t espfsp_comm_proto_session_ping(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_session_ping_message_t *msg);
esp_err_t espfsp_comm_proto_start_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_start_stream_message_t *msg);
esp_err_t espfsp_comm_proto_stop_stream(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_req_stop_stream_message_t *msg);
// Actions for requests --- END

// Actions for responses --- BEGIN
esp_err_t espfsp_comm_proto_session_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_ack_message_t *msg);
esp_err_t espfsp_comm_proto_session_pong(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_session_pong_message_t *msg);
esp_err_t espfsp_comm_proto_ack(espfsp_comm_proto_t *comm_proto, espfsp_comm_proto_resp_ack_message_t *msg);
// Actions for responses --- END

// Additional methods --- BEGIN
// esp_err_t espfsp_comm_proto_cam_set_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id, uint32_t value);
// esp_err_t espfsp_comm_proto_cam_get_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id);
// esp_err_t espfsp_comm_proto_frame_set_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id, uint32_t value);
// esp_err_t espfsp_comm_proto_frame_get_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id);
// esp_err_t espfsp_comm_proto_proto_set_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id, uint32_t value);
// esp_err_t espfsp_comm_proto_proto_get_params(espfsp_comm_proto_t *comm_proto, uint32_t session_id, uint16_t param_id);
// esp_err_t espfsp_comm_proto_source_set(espfsp_comm_proto_t *comm_proto, uint32_t session_id, char source_id[16]);
// esp_err_t espfsp_comm_proto_source_get(espfsp_comm_proto_t *comm_proto, uint32_t session_id);
// Additional methods --- END
