/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "client_push/espfsp_state_def.h"
#include "client_push/espfsp_comm_proto_handlers.h"
#include "comm_proto/espfsp_comm_proto.h"

#include "client_push/espfsp_comm_proto_conf.h"

// static const char *TAG = "ESPFSP_CLIENT_PUSH_COMM_PROTO_CONF";

esp_err_t espfsp_client_push_comm_protos_init(espfsp_client_push_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_config_t config;

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = espfsp_client_push_req_session_terminate_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_START_STREAM] = espfsp_client_push_req_start_stream_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_STOP_STREAM] = espfsp_client_push_req_stop_stream_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_CAM_SET_PARAMS] = espfsp_client_push_req_cam_set_params_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_FRAME_SET_PARAMS] = espfsp_client_push_req_frame_set_params_handler;
    config.resp_callbacks[ESPFSP_COMM_RESP_SESSION_ACK] = espfsp_client_push_resp_session_ack_handler;
    config.repetive_callback = NULL;
    config.repetive_callback_freq_us = 100000000;
    config.conn_closed_callback = espfsp_client_push_connection_stop;
    config.conn_reset_callback = espfsp_client_push_connection_stop;
    config.conn_term_callback = espfsp_client_push_connection_stop;

    ret = espfsp_comm_proto_init(&instance->comm_proto, &config);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ret;
}

esp_err_t espfsp_client_push_comm_protos_deinit(espfsp_client_push_instance_t *instance)
{
    return espfsp_comm_proto_deinit(&instance->comm_proto);
}
