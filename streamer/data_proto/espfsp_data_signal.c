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

static esp_err_t get_last_signal(int sock, uint8_t *signal, struct sockaddr_in *addr, socklen_t *addr_len)
{
    esp_err_t ret = ESP_OK;
    int received_bytes = 0;

    ret = espfsp_receive_bytes_from(sock, (char *) signal, sizeof(uint8_t), addr, addr_len);
    if (ret != ESP_OK)
        return ret;

    // With high probability, the lates receied packate will be the last one that went through NAT, so NAT mapping
    // for this last packet should be kept in NAT with high probability
    do
    {
        ret = espfsp_receive_from_no_block(sock, (char *) signal, sizeof(uint8_t), &received_bytes, addr, addr_len);
        if (ret != ESP_OK)
            return ret;
    } while (received_bytes > 0);

    return ret;
}

static bool should_signal_be_handled(espfsp_data_proto_t *data_proto, uint64_t current_time)
{
    return data_proto->last_traffic == NO_SIGNAL ||
           (current_time - data_proto->last_traffic) >= MAX_TIME_US_NO_NAT_TRAVERSAL;
}

esp_err_t espfsp_data_proto_handle_incoming_signal(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    uint64_t current_time = esp_timer_get_time();
    uint8_t received_signal = SIGNAL_VAL_NOK;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_UNSPEC;
    socklen_t addr_len = sizeof(addr);

    if (should_signal_be_handled(data_proto, current_time))
    {
        ret = espfsp_connect(sock, &addr); // Disconnect
        if (ret != ESP_OK)
            return ret;

        ret = get_last_signal(sock, &received_signal, &addr, &addr_len);
        if (ret != ESP_OK)
            return ret;

        if (received_signal != SIGNAL_VAL_OK)
            return ESP_FAIL;

        ret = espfsp_connect(sock, &addr);
        if (ret != ESP_OK)
            return ret;

        data_proto->last_traffic = current_time;
    }

    return ret;
}

esp_err_t espfsp_data_proto_handle_outcoming_signal(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;
    uint64_t current_time = esp_timer_get_time();
    uint8_t signals_to_send = SIGNALS_TO_SEND;
    uint8_t signal = SIGNAL_VAL_OK;

    if(should_signal_be_handled(data_proto, current_time))
    {
        while (signals_to_send-- > 0)
        {
            ret = espfsp_send(sock, (char *)&signal, sizeof(signal));
            if (ret != ESP_OK)
                return ret;
        }

        data_proto->last_traffic = current_time;
    }

    return ret;
}
