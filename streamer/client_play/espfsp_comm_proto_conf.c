/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "client_play/espfsp_state_def.h"
#include "client_play/espfsp_comm_proto_handlers.h"
#include "comm_proto/espfsp_comm_proto.h"

#include "client_play/espfsp_comm_proto_conf.h"

static const char *TAG = "ESPFSP_CLIENT_PLAY_COMM_PROTO_CONF";

esp_err_t espfsp_client_play_comm_protos_init(espfsp_client_play_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    espfsp_comm_proto_config_t config;

    config.callback_ctx = (void *) instance,
    config.buffered_actions = 3,

    memset(config.req_callbacks, 0, sizeof(config.req_callbacks));
    memset(config.resp_callbacks, 0, sizeof(config.resp_callbacks));

    config.req_callbacks[ESPFSP_COMM_REQ_SESSION_TERMINATE] = espfsp_client_play_req_session_terminate_handler;
    config.resp_callbacks[ESPFSP_COMM_RESP_SESSION_ACK] = espfsp_client_play_resp_session_ack_handler;
    config.repetive_callback = NULL;

    ret = espfsp_comm_proto_init(&instance->comm_proto, &config);
    if (ret != ESP_OK)
    {
        return ret;
    }

    espfsp_comm_proto_req_session_init_message_t msg = {
        .client_type = ESPFSP_COMM_REQ_CLIENT_PLAY,
    };

    return espfsp_comm_proto_session_init(&instance->comm_proto, &msg);
}

esp_err_t espfsp_client_play_comm_protos_deinit(espfsp_client_play_instance_t *instance)
{
    return espfsp_comm_proto_deinit(&instance->comm_proto);
}
