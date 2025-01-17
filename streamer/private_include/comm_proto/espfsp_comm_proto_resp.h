/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <stdint.h>

// Types has to have assigned consecutive, smallest possible values. These values are used to index table
typedef enum {
    ESPFSP_COMM_RESP_SESSION_ACK = 0x01,
    ESPFSP_COMM_RESP_SESSION_PONG = 0x02,
    ESPFSP_COMM_RESP_ACK = 0x03,
    ESPFSP_COMM_RESP_CAM_PARAMS_RESP = 0x04,
    ESPFSP_COMM_RESP_FRAME_PARAMS_RESP = 0x05,
    ESPFSP_COMM_RESP_SOURCES_RESP = 0x06,

    ESPFSP_COMM_RESP_MAX_NUMBER = 0x07,

    // ESPFSP_COMM_RESP_STREAM_STATUS = 0xXX,
    // ESPFSP_COMM_RESP_ERROR_REPORT = 0xXX,
} espfsp_comm_proto_resp_type_t;

// For ESPFSP_COMM_RESP_SESSION_ACK
typedef struct {
    uint32_t session_id;
} espfsp_comm_proto_resp_session_ack_message_t;

// For ESPFSP_COMM_RESP_SESSION_PONG
typedef struct {
    uint32_t session_id;
    uint32_t timestamp;
} espfsp_comm_proto_resp_session_pong_message_t;

// For ESPFSP_COMM_RESP_ACK
typedef struct {
    uint32_t session_id;
} espfsp_comm_proto_resp_ack_message_t;

// // For ESPFSP_COMM_RESP_CAM_PARAMS_RESP
typedef struct {
    uint32_t session_id;
    uint16_t param_id;
    uint32_t value;
} espfsp_comm_resp_cam_params_resp_message_t;

// // For ESPFSP_COMM_RESP_FRAME_PARAMS_RESP
typedef struct {
    uint32_t session_id;
    uint16_t param_id;
    uint32_t value;
} espfsp_comm_resp_frame_params_resp_message_t;

// // For ESPFSP_COMM_RESP_SOURCES_RESP
typedef struct {
    uint32_t session_id;
    uint8_t num_sources;
    char source_names[3][30];
} espfsp_comm_resp_sources_resp_message_t;

// // For ESPFSP_COMM_RESP_ERROR_REPORT
// typedef struct {
//     uint32_t session_id;
//     uint16_t error_code;
//     char error_message[64];
// } espfsp_comm_req_error_report_message_t;
