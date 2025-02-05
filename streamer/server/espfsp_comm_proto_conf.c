/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "espfsp_message_defs.h"
#include "server/espfsp_state_def.h"
#include "server/espfsp_comm_proto_handlers.h"
#include "comm_proto/espfsp_comm_proto.h"

#include "server/espfsp_comm_proto_conf.h"

// static const char *TAG = "ESPFSP_SERVER_COMM_PROTO_CONF";

esp_err_t espfsp_server_comm_protos_init(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_config_t config;

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_INIT] = espfsp_server_req_session_init_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = espfsp_server_req_session_terminate_handler;
    config.repetive_callback = NULL;
    config.repetive_callback_freq_us = 100000000;
    config.conn_closed_callback = espfsp_server_connection_stop;
    config.conn_reset_callback = espfsp_server_connection_stop;
    config.conn_term_callback = espfsp_server_connection_stop;


    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS; i++)
    {
        ret = espfsp_comm_proto_init(&instance->client_push_comm_proto[i], &config);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_INIT] = espfsp_server_req_session_init_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = espfsp_server_req_session_terminate_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_START_STREAM] = espfsp_server_req_start_stream_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_STOP_STREAM] = espfsp_server_req_stop_stream_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_CAM_SET_PARAMS] = espfsp_server_req_cam_set_params_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_FRAME_SET_PARAMS] = espfsp_server_req_frame_set_params_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SOURCE_SET] = espfsp_server_req_source_set_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SOURCE_GET] = espfsp_server_req_source_get_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_FRAME_GET_PARAMS] = espfsp_server_req_frame_get_params_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_CAM_GET_PARAMS] = espfsp_server_req_cam_get_params_handler;
    config.repetive_callback = NULL;
    config.repetive_callback_freq_us = 100000000;
    config.conn_closed_callback = espfsp_server_connection_stop;
    config.conn_reset_callback = espfsp_server_connection_stop;
    config.conn_term_callback = espfsp_server_connection_stop;

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS; i++)
    {
        ret = espfsp_comm_proto_init(&instance->client_play_comm_proto[i], &config);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    return ret;
}

esp_err_t espfsp_server_comm_protos_deinit(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS; i++)
    {
        ret = espfsp_comm_proto_deinit(&instance->client_play_comm_proto[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    for (int i = 0; i < CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS; i++)
    {
        ret = espfsp_comm_proto_deinit(&instance->client_push_comm_proto[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    return ret;
}
