/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "comm_proto/espfsp_comm_proto.h"

esp_err_t espfsp_server_req_session_init_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_session_terminate_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_start_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_stop_stream_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_cam_set_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_cam_get_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_frame_set_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_frame_get_params_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_source_set_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);
esp_err_t espfsp_server_req_source_get_handler(espfsp_comm_proto_t *comm_proto, void *msg_content, void *ctx);

esp_err_t espfsp_server_connection_stop(espfsp_comm_proto_t *comm_proto, void *ctx);


// esp_err_t espfsp_server_req_stream_status_handler(espfsp_server_instance_t *instance);
// esp_err_t espfsp_server_req_error_report_handler(espfsp_server_instance_t *instance);
// esp_err_t espfsp_server_req_proto_set_params_handler(espfsp_server_instance_t *instance);
// esp_err_t espfsp_server_req_proto_get_params_handler(espfsp_server_instance_t *instance);
