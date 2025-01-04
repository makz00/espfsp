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

static const char *TAG = "ESPFSP_SERVER_COMM_PROTO_CONF";

esp_err_t espfsp_server_comm_protos_init(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_config_t config;

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_INIT] = req_session_init_server_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = req_session_terminate_server_handler;
    config.repetive_callback = NULL;

    ret = espfsp_comm_proto_init(&instance->client_push_comm_proto, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol initiation failed");
        return ret;
    }

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_INIT] = req_session_init_server_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = req_session_terminate_server_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_START_STREAM] = req_start_stream_server_handler;
    config.req_callbacks[ESPFSP_COMM_REQ_STOP_STREAM] = req_stop_stream_server_handler;
    config.repetive_callback = NULL;

    ret = espfsp_comm_proto_init(&instance->client_play_comm_proto, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Communication protocol initiation failed");
        return ret;
    }

    return ret;
}

esp_err_t espfsp_server_comm_protos_deinit(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    ret = espfsp_comm_proto_deinit(&instance->client_play_comm_proto);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = espfsp_comm_proto_deinit(&instance->client_push_comm_proto);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ret;
}
