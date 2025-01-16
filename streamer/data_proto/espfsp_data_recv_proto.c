/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stddef.h>

#include "esp_timer.h"

#include "espfsp_message_buffer.h"
#include "espfsp_sock_op.h"
#include "data_proto/espfsp_data_signal.h"
#include "data_proto/espfsp_data_recv_proto.h"

static const char *TAG = "ESPFSP_DATA_RECIVE_PROTOCOL";

static struct timeval recv_timeout = {
    .tv_sec = 0,
    .tv_usec = MAX_TIME_US_NO_MSG_RECEIVED,
};

static esp_err_t recv_msg(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    char rx_buffer[sizeof(espfsp_message_t)];
    int received = 0;

    // This function receive data that are sent with UDP, so receive 0 bytes can happen.
    // We cannot block on this call as maybe another NAT hole punch is required to receive data.
    ret = espfsp_receive_block(sock, rx_buffer, sizeof(espfsp_message_t), &received, &recv_timeout);
    if (ret == ESP_OK && received > 0)
    {
        // ESP_LOGI(
        //     TAG,
        //     "Received msg part: %d/%d for timestamp: sek: %lld, usek: %ld",
        //     ((espfsp_message_t *)rx_buffer)->msg_number,
        //     ((espfsp_message_t *)rx_buffer)->msg_total,
        //     ((espfsp_message_t *)rx_buffer)->timestamp.tv_sec,
        //     ((espfsp_message_t *)rx_buffer)->timestamp.tv_usec);

        espfsp_message_buffer_process_message((espfsp_message_t *)rx_buffer, data_proto->config->recv_buffer);
        data_proto->last_traffic = esp_timer_get_time();
    }

    return ret;
}

esp_err_t espfsp_data_proto_handle_recv(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;

    if (data_proto->config->mode == ESPFSP_DATA_PROTO_MODE_NAT)
    {
        ret = espfsp_data_proto_handle_outcoming_signal(data_proto, sock);
    }
    if (ret == ESP_OK)
    {
        ret = recv_msg(data_proto, sock);
    }

    return ret;
}
