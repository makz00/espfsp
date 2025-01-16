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

static esp_err_t send_frame(espfsp_fb_t *fb, void *ctx, espfsp_data_proto_send_frame_state_t *state)
{
    esp_err_t ret = ESP_OK;
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) ctx;
    espfsp_send_frame_cb_state_t send_frame_cb_state = ESPFSP_SEND_FRAME_CB_FRAME_NOT_OBTAINED;

    ret = instance->config->cb.send_frame(fb, &send_frame_cb_state);
    if (ret == ESP_OK)
    {
        switch (send_frame_cb_state)
        {
        case ESPFSP_SEND_FRAME_CB_FRAME_OBTAINED:
            *state = ESPFSP_DATA_PROTO_FRAME_OBTAINED;
            break;

        case ESPFSP_SEND_FRAME_CB_FRAME_NOT_OBTAINED:
            *state = ESPFSP_DATA_PROTO_FRAME_NOT_OBTAINED;
            break;

        default:
            ESP_LOGE(TAG, "Send frame CB state not handled");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_client_push_data_protos_init(espfsp_client_push_instance_t *instance)
{
    espfsp_data_proto_config_t config;

    config.type = ESPFSP_DATA_PROTO_TYPE_SEND;
    config.mode = ESPFSP_DATA_PROTO_MODE_LOCAL;
    config.recv_buffer = NULL;
    config.send_frame_callback = send_frame;
    config.send_frame_ctx = instance;
    config.frame_config = &instance->config->frame_config;

    return espfsp_data_proto_init(&instance->data_proto, &config);
}

esp_err_t espfsp_client_push_data_protos_deinit(espfsp_client_push_instance_t *instance)
{
    return espfsp_data_proto_deinit(&instance->data_proto);
}
