/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "comm_proto/espfsp_comm_proto.h"

esp_err_t espfsp_client_play_req_session_terminate_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_play_req_stop_stream_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_play_resp_session_ack_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_play_resp_sources_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_play_resp_frame_config_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_client_play_resp_cam_config_handler(
    espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);

esp_err_t espfsp_client_play_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx);
