/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_message_buffer.h"
#include "server/espfsp_state_def.h"
#include "data_proto/espfsp_data_proto.h"

#include "server/espfsp_data_proto_conf.h"

static const char *TAG = "ESPFSP_SERVER_DATA_PROTO_CONF";

static esp_err_t send_frame(espfsp_fb_t *fb, void *ctx, espfsp_data_proto_send_frame_state_t *state)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) ctx;
    espfsp_fb_t *recv_buf_fb = NULL;

    recv_buf_fb = espfsp_message_buffer_get_fb(&instance->receiver_buffer, 200);
    if (recv_buf_fb == NULL)
    {
        *state = ESPFSP_DATA_PROTO_FRAME_NOT_OBTAINED;
        return ESP_OK;
    }

    fb->len = recv_buf_fb->len;
    fb->width = recv_buf_fb->width;
    fb->height = recv_buf_fb->height;
    fb->format = recv_buf_fb->format;
    fb->timestamp.tv_sec = recv_buf_fb->timestamp.tv_sec;
    fb->timestamp.tv_usec = recv_buf_fb->timestamp.tv_usec;
    memcpy(fb->buf, recv_buf_fb->buf, recv_buf_fb->len);

    ret = espfsp_message_buffer_return_fb(&instance->receiver_buffer);
    if (ret == ESP_OK)
    {
        *state = ESPFSP_DATA_PROTO_FRAME_OBTAINED;
    }

    return ret;
}

esp_err_t espfsp_server_data_protos_init(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;
    espfsp_data_proto_config_t config;

    config.type = ESPFSP_DATA_PROTO_TYPE_RECV;
    config.mode = ESPFSP_DATA_PROTO_MODE_LOCAL;
    config.recv_buffer = &instance->receiver_buffer;
    config.send_fb = NULL;
    config.send_frame_callback = NULL;
    config.send_frame_ctx = NULL;
    config.frame_config = &instance->config->frame_config;

    ret = espfsp_data_proto_init(&instance->client_push_data_proto, &config);
    if (ret != ESP_OK)
    {
        return ret;
    }

    config.type = ESPFSP_DATA_PROTO_TYPE_SEND;
    config.mode = ESPFSP_DATA_PROTO_MODE_NAT;
    config.recv_buffer = NULL;
    config.send_fb = &instance->sender_frame;
    config.send_frame_callback = send_frame;
    config.send_frame_ctx = instance;
    config.frame_config = &instance->config->frame_config;

    return espfsp_data_proto_init(&instance->client_play_data_proto, &config);
}

esp_err_t espfsp_server_data_protos_deinit(espfsp_server_instance_t *instance)
{
    esp_err_t ret = ESP_OK;

    ret = espfsp_data_proto_deinit(&instance->client_play_data_proto);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return espfsp_data_proto_deinit(&instance->client_push_data_proto);
}
