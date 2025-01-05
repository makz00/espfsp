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

// esp_err_t req_cam_set_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_cam_get_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_frame_set_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_frame_get_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_proto_set_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_proto_get_params_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_source_set_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_source_get_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_stream_status_server_handler(espfsp_server_instance_t *instance);
// esp_err_t req_error_report_server_handler(espfsp_server_instance_t *instance);
