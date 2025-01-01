/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <stdint.h>

// Types has to have assigned consecutive, smallest possible values. These values are used to index table
typedef enum {
    ESPFSP_COMM_REQ_SESSION_INIT = 0x01,
    ESPFSP_COMM_REQ_SESSION_TERMINATE = 0x02,
    ESPFSP_COMM_REQ_SESSION_PING = 0x03,
    ESPFSP_COMM_REQ_START_STREAM = 0x04,
    ESPFSP_COMM_REQ_STOP_STREAM = 0x05,

    ESPFSP_COMM_REQ_MAX_NUMBER = 0x06,

    // ESPFSP_COMM_REQ_CAM_SET_PARAMS = 0x06,
    // ESPFSP_COMM_REQ_CAM_GET_PARAMS = 0x07,
    // ESPFSP_COMM_REQ_FRAME_SET_PARAMS = 0x08,
    // ESPFSP_COMM_REQ_FRAME_GET_PARAMS = 0x09,
    // ESPFSP_COMM_REQ_PROTO_SET_PARAMS = 0x0A,
    // ESPFSP_COMM_REQ_PROTO_GET_PARAMS = 0x0B,
    // ESPFSP_COMM_REQ_SOURCE_SET = 0x0C,
    // ESPFSP_COMM_REQ_SOURCE_GET = 0x0D,
    // ESPFSP_COMM_REQ_STREAM_STATUS = 0x0E,
    // ESPFSP_COMM_REQ_MAX_NUMBER = 0x0F,
} espfsp_comm_proto_req_type_t;

typedef enum {
    ESPFSP_COMM_REQ_CLIENT_PUSH = 0x01,
    ESPFSP_COMM_REQ_CLIENT_PLAY = 0x02,
} espfsp_comm_proto_req_client_type_t;

// For ESPFSP_COMM_REQ_SESSION_INIT
typedef struct {
    espfsp_comm_proto_req_client_type_t client_type;
} espfsp_comm_proto_req_session_init_message_t;

// For ESPFSP_COMM_REQ_SESSION_TERMINATE
typedef struct {
    uint32_t session_id;
} espfsp_comm_proto_req_session_terminate_message_t;

// For ESPFSP_COMM_REQ_SESSION_PING
typedef struct {
    uint32_t session_id;
    uint32_t timestamp;
} espfsp_comm_proto_req_session_ping_message_t;

// For ESPFSP_COMM_REQ_START_STREAM
typedef struct {
    uint32_t session_id;
} espfsp_comm_proto_req_start_stream_message_t;

// For ESPFSP_COMM_REQ_STOP_STREAM
typedef struct {
    uint32_t session_id;
} espfsp_comm_proto_req_stop_stream_message_t;

// // For ESPFSP_COMM_REQ_CAM_SET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
//     uint32_t value;
// } espfsp_comm_req_cam_set_param_message_t;

// // For ESPFSP_COMM_REQ_CAM_GET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
// } espfsp_comm_req_cam_get_param_message_t;

// // For ESPFSP_COMM_REQ_FRAME_SET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
//     uint32_t value;
// } espfsp_comm_req_frame_set_param_message_t;

// // For ESPFSP_COMM_REQ_FRAME_GET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
// } espfsp_comm_req_frame_get_param_message_t;

// // For ESPFSP_COMM_REQ_PROTO_SET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
//     uint32_t value;
// } espfsp_comm_req_proto_set_param_message_t;

// // For ESPFSP_COMM_REQ_PROTO_GET_PARAM
// typedef struct {
//     uint32_t session_id;
//     uint16_t param_id;
// } espfsp_comm_req_proto_get_param_message_t;

// // For ESPFSP_COMM_REQ_SOURCE_SET
// typedef struct {
//     uint32_t session_id;
//     char source_id[16];
// } espfsp_comm_req_source_set_message_t;

// // For ESPFSP_COMM_REQ_SOURCE_GET
// typedef struct {
//     uint32_t session_id;
// } espfsp_comm_req_source_get_message_t;

// // For ESPFSP_COMM_REQ_STREAM_STATUS
// typedef struct {
//     uint32_t session_id;
//     uint8_t stream_id;
//     uint8_t frame_loss;
//     uint16_t latency_ms;
// } espfsp_comm_req_stream_status_message_t;
