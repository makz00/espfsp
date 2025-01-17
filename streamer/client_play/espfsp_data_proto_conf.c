/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "client_play/espfsp_state_def.h"
#include "data_proto/espfsp_data_proto.h"

#include "client_play/espfsp_data_proto_conf.h"

// static const char *TAG = "ESPFSP_CLIENT_PLAY_DATA_PROTO_CONF";

esp_err_t espfsp_client_play_data_protos_init(espfsp_client_play_instance_t *instance)
{
    espfsp_data_proto_config_t config;

    config.type = ESPFSP_DATA_PROTO_TYPE_RECV;
    config.mode = ESPFSP_DATA_PROTO_MODE_NAT;
    config.recv_buffer = &instance->receiver_buffer;
    config.send_frame_callback = NULL;
    config.send_frame_ctx = NULL;
    config.frame_config = &instance->config->frame_config;

    return espfsp_data_proto_init(&instance->data_proto, &config);
}

esp_err_t espfsp_client_play_data_protos_deinit(espfsp_client_play_instance_t *instance)
{
    return espfsp_data_proto_deinit(&instance->data_proto);
}
