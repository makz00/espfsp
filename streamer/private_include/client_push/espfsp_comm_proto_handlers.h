/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "comm_proto/espfsp_comm_proto.h"

esp_err_t espfsp_client_push_req_session_terminate_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_start_stream_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_stop_stream_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_resp_session_ack_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_cam_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_cam_get_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_frame_set_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_push_req_frame_get_params_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);

esp_err_t espfsp_client_push_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx);
