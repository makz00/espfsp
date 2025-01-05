/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "client_push/espfsp_state_def.h"
#include "data_proto/espfsp_data_proto.h"

#include "client_push/espfsp_data_proto_conf.h"

static const char *TAG = "ESPFSP_CLIENT_PUSH_DATA_PROTO_CONF";

static esp_err_t send_frame(espfsp_fb_t *fb, void *ctx)
{
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;
    return instance->config->cb.send_frame(fb);
}

esp_err_t espfsp_client_push_data_protos_init(espfsp_client_push_instance_t *instance)
{
    espfsp_data_proto_config_t config;

    config.type = ESPFSP_DATA_PROTO_TYPE_SEND;
    config.mode = ESPFSP_DATA_PROTO_MODE_LOCAL;
    config.recv_buffer = NULL;
    config.send_fb = &instance->sender_frame;
    config.send_frame_callback = send_frame;
    config.send_frame_ctx = instance;

    return espfsp_data_proto_init(&instance->data_proto, &config);
}

esp_err_t espfsp_client_push_data_protos_deinit(espfsp_client_push_instance_t *instance)
{
    return espfsp_data_proto_deinit(&instance->data_proto);
}
