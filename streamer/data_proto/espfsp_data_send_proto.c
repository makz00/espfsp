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
    uint64_t current_time = esp_timer_get_time();

    ret = espfsp_send_whole_fb_within(sock, send_fb, data_proto->frame_interval_us);
    if (ret == ESP_OK)
    {
        // ESP_LOGI(TAG, "Interval time: %lldms", data_proto->frame_interval_us >> 10);
        // ESP_LOGI(TAG, "Send time: %lldms", (current_time - data_proto->last_traffic) >> 10);

        data_proto->last_traffic = current_time;
    }

    return ret;
}

esp_err_t espfsp_data_proto_handle_send(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    espfsp_data_proto_send_frame_state_t frame_state = ESPFSP_DATA_PROTO_FRAME_NOT_OBTAINED;
    bool host_connected = true;

    // Potentially send_frame_callback() could block for some time to wait for FB as
    // - FB can be taken from camera directly OR
    // - FB can be taken from Receiver Buffer (waits for frames from internet)
    // depending on configuration.
    // In order to not block, it shall return after some short time in order to not trigger WD.
    // It is best not to block at all in this callback.
    ret = data_proto->config->send_frame_callback(
        &data_proto->send_fb, data_proto->config->send_frame_ctx, &frame_state);

    if (data_proto->send_fb.len > data_proto->frame_config.frame_max_len)
    {
        ESP_LOGE(TAG, "Frame to send size is greater than allocated buffer");
        return ret;
    }

    if (ret == ESP_OK &&
        frame_state == ESPFSP_DATA_PROTO_FRAME_OBTAINED &&
        data_proto->config->mode == ESPFSP_DATA_PROTO_MODE_NAT)
    {
        host_connected = false;
        ret = espfsp_data_proto_handle_incoming_signal(data_proto, sock, &host_connected);
    }
    if (ret == ESP_OK && frame_state == ESPFSP_DATA_PROTO_FRAME_OBTAINED && host_connected)
    {
        ret = send_fb(data_proto, sock, &data_proto->send_fb);
    }

    return ret;
}
