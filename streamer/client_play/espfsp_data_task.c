/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_netif.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_message_defs.h"
#include "espfsp_client_play.h"
#include "espfsp_sock_op.h"
#include "espfsp_message_buffer.h"
#include "client_play/espfsp_data_task.h"
#include "client_play/espfsp_state_def.h"

static const char *TAG = "ESPFSP_CLIENT_PLAY_DATA_TASK";

static const char *BULLET_PAYLOAD = "BULLET";

static void punch_hole_in_nat(int sock)
{
    for (int i = 0; i < 10; i++)
    {
        while (send(sock, BULLET_PAYLOAD, strlen(BULLET_PAYLOAD), 0) <= 0) {}
    }
}

static void process_receiver_connection(int sock, espfsp_client_play_instance_t *instance)
{
    char rx_buffer[sizeof(espfsp_message_t)];

    while (1)
    {
        punch_hole_in_nat(sock);

        int ret = espfsp_receive_bytes(sock, rx_buffer, sizeof(espfsp_message_t));
        if (ret > 0)
        {
            // ESP_LOGI(
            //     TAG,
            //     "Received msg part: %d/%d for timestamp: sek: %lld, usek: %ld",
            //     ((espfsp_message_t *)rx_buffer)->msg_number,
            //     ((espfsp_message_t *)rx_buffer)->msg_total,
            //     ((espfsp_message_t *)rx_buffer)->timestamp.tv_sec,
            //     ((espfsp_message_t *)rx_buffer)->timestamp.tv_usec);

            espfsp_message_buffer_process_message((espfsp_message_t *)rx_buffer, &instance->receiver_buffer);
        }
        else
        {
            ESP_LOGE(TAG, "Cannot receive message. End receiver process");
            break;
        }
    }
}

void streamer_client_receiver_task(void *pvParameters)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) pvParameters;
    const espfsp_client_play_config_t *config = instance->config;

    struct esp_ip4_addr remote_addr;
    remote_addr.addr = inet_addr(config->host_ip);

    struct sockaddr_in dest_addr;
    espfsp_set_addr(&dest_addr, &remote_addr, config->remote.data_port);

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_client(&sock, config->local.data_port, &dest_addr);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP client failed");
            continue;
        }

        ESP_LOGI(TAG, "Start process connection");

        process_receiver_connection(sock, instance);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove UDP client failed");
            break;
        }
    }

    vTaskDelete(NULL);
}
