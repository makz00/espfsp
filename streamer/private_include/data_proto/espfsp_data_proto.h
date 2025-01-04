/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include <stdint.h>
#include <stddef.h>

#include "espfsp_config.h"
#include "espfsp_message_buffer.h"
#include "comm_proto/espfsp_comm_proto.h"

#define MAX_TIME_US_NO_NAT_TRAVERSAL 5000000 // 5 seconds
#define MAX_TIME_US_NO_MSG_RECEIVED  2000000 // 2 seconds

#define NO_SIGNAL 0

#define SIGNAL_VAL_NOK  0
#define SIGNAL_VAL_OK   1
#define SIGNALS_TO_SEND 10

typedef enum {
    ESPFSP_DATA_PROTO_TYPE_SEND,
    ESPFSP_DATA_PROTO_TYPE_RECV,
} espfsp_data_proto_type_t;

typedef enum {
    ESPFSP_DATA_PROTO_MODE_LOCAL,
    ESPFSP_DATA_PROTO_MODE_NAT,
} espfsp_data_proto_mode_t;

typedef enum {
    ESPFSP_DATA_PROTO_STATE_LOOP,
    ESPFSP_DATA_PROTO_STATE_CONTROL,
    ESPFSP_DATA_PROTO_STATE_ERROR,
} espfsp_data_proto_state_t;

typedef esp_err_t (*__espfsp_data_proto_send_frame)(espfsp_fb_t *fb, void *ctx);

typedef struct {
    espfsp_data_proto_type_t type;
    espfsp_data_proto_mode_t mode;
    espfsp_receiver_buffer_t *recv_buffer;                  // Receiver buffer has to be configured; It is not managed by Data Protocol
    espfsp_fb_t *send_fb;                                   // Send FB has to be configured; It is not managed by Data Protocol
    __espfsp_data_proto_send_frame send_frame_callback;     // Callback to obtain FB that will be sent by Data Protocol
    void *send_frame_ctx;
} espfsp_data_proto_config_t;

typedef struct {
    espfsp_data_proto_config_t *config;
    espfsp_data_proto_state_t state;
    uint64_t last_traffic;
    uint8_t en;
} espfsp_data_proto_t;

esp_err_t espfsp_data_proto_init(espfsp_data_proto_t *data_proto, espfsp_data_proto_config_t *config);
esp_err_t espfsp_data_proto_deinit(espfsp_data_proto_t *data_proto);

// Data Receiver Protocol has to be associated with one running Communication Protocol.
esp_err_t espfsp_data_proto_run(espfsp_data_proto_t *data_proto, int sock, espfsp_comm_proto_t *comm_proto);
esp_err_t espfsp_data_proto_stop(espfsp_data_proto_t *data_proto);
