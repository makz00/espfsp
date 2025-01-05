/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stddef.h>

#include "esp_timer.h"

#include "espfsp_config.h"
#include "espfsp_sock_op.h"
#include "data_proto/espfsp_data_signal.h"
#include "data_proto/espfsp_data_send_proto.h"

static const char *TAG = "ESPFSP_DATA_SEND_PROTOCOL";

static esp_err_t send_fb(espfsp_data_proto_t *data_proto, int sock, espfsp_fb_t *send_fb)
{
    esp_err_t ret = ESP_OK;

    ret = espfsp_send_whole_fb(sock, send_fb);
    if (ret != ESP_OK)
        return ret;

    data_proto->last_traffic = esp_timer_get_time();
    return ret;
}

static esp_err_t get_fb(espfsp_data_proto_t *data_proto, espfsp_fb_t *send_fb)
{
    // Potentially send_frame_callback() could block for some time to wait for FB as
    // - FB can be taken from camera directly OR
    // - FB can be taken from Receiver Buffer (waits for frames from internet)
    // depending on configuration.
    return data_proto->config->send_frame_callback(send_fb, data_proto->config->send_frame_ctx);
}

esp_err_t espfsp_data_proto_handle_send(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;

    ret = get_fb(data_proto, data_proto->config->send_fb);
    if (ret != ESP_OK)
        return ret;

    switch (data_proto->config->mode)
    {
    case ESPFSP_DATA_PROTO_MODE_NAT:

        ret = espfsp_data_proto_handle_incoming_signal(data_proto, sock);
        if (ret != ESP_OK)
            return ret;

    default:
    }

    return send_fb(data_proto, sock, data_proto->config->send_fb);
}
