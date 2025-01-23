/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stddef.h>

#include "esp_timer.h"
#include "lwip/sockets.h"

#include "espfsp_sock_op.h"
#include "data_proto/espfsp_data_proto.h"
#include "data_proto/espfsp_data_signal.h"

// static const char *TAG = "ESPFSP_DATA_PROT_SIGNAL";

static struct timeval recv_timeout = {
    .tv_sec = 0,
    .tv_usec = MAX_TIME_US_NO_MSG_RECEIVED,
};

static esp_err_t get_last_signal(int sock, uint8_t *signal, struct sockaddr_in *addr, socklen_t *addr_len)
{
    esp_err_t ret = ESP_OK;
    int received_bytes = 0;

    ret = espfsp_receive_from_block(
        sock, (char *) signal, sizeof(uint8_t), &received_bytes, &recv_timeout, addr, addr_len);
    if (ret == ESP_OK && received_bytes == 0)
    {
        *signal = NAT_NO_SIGNAL_VAL;
    }
    if (ret == ESP_OK && received_bytes >= 0)
    {
        // With high probability, the lates received packet will be the last one that went through NAT, so NAT mapping
        // for this last packet should be kept in NAT with high probability
        do
        {
            ret = espfsp_receive_from_no_block(sock, (char *) signal, sizeof(uint8_t), &received_bytes, addr, addr_len);
            if (ret != ESP_OK)
                break;
        } while (received_bytes > 0);
    }

    return ret;
}

static bool should_signal_be_handled(espfsp_data_proto_t *data_proto, uint64_t current_time)
{
    return data_proto->last_traffic == TRAFFIC_NO_SIGNAL ||
           (current_time - data_proto->last_traffic) >= MAX_TIME_US_NO_NAT_TRAVERSAL;
}

esp_err_t espfsp_data_proto_handle_incoming_signal(espfsp_data_proto_t *data_proto, int sock, bool *connected)
{
    esp_err_t ret = ESP_OK;
    uint64_t current_time = esp_timer_get_time();
    uint8_t received_signal = NAT_SIGNAL_VAL_NOK;
    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);
    *connected = true;

    if (should_signal_be_handled(data_proto, current_time))
    {
        // We assume that NAT entry could gone away and we need to make new one
        *connected = false;
        addr.sin_family = AF_UNSPEC;

        ret = espfsp_connect(sock, &addr); // Disconnect socket
        if (ret == ESP_OK)
        {
            ret = get_last_signal(sock, &received_signal, &addr, &addr_len);
        }
        if (ret == ESP_OK && received_signal == NAT_SIGNAL_VAL_NOK)
        {
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK && received_signal == NAT_SIGNAL_VAL_OK)
        {
            ret = espfsp_connect(sock, &addr);
        }
        if (ret == ESP_OK && received_signal == NAT_SIGNAL_VAL_OK)
        {
            *connected = true;
            data_proto->last_traffic = current_time;
        }
    }

    return ret;
}

esp_err_t espfsp_data_proto_handle_outcoming_signal(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    uint64_t current_time = esp_timer_get_time();
    uint8_t signals_to_send = SIGNALS_TO_SEND;
    uint8_t signal = NAT_SIGNAL_VAL_OK;

    if(should_signal_be_handled(data_proto, current_time))
    {
        while (signals_to_send-- > 0)
        {
            if (ret == ESP_OK)
            {
                ret = espfsp_send(sock, (char *)&signal, sizeof(signal));
            }
        }
        if (ret == ESP_OK)
        {
            data_proto->last_traffic = current_time;
        }
    }

    return ret;
}
