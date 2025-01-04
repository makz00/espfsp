/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_netif.h"
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_sock_op.h"
#include "espfsp_message_buffer.h"
#include "server/espfsp_client_push_data_task.h"
#include "server/espfsp_state_def.h"

static const char *TAG = "ESPFSP_SERVER_CLIENT_PUSH_DATA_TASK";

static void process_receiver_connection(int sock, espfsp_server_instance_t *instance)
{
    char rx_buffer[sizeof(espfsp_message_t)];

    while (1)
    {
        esp_err_t ret = espfsp_receive_bytes(sock, rx_buffer, sizeof(espfsp_message_t));
        if (ret == ESP_OK)
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

void espfsp_server_client_push_data_task(void *pvParameters)
{
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) pvParameters;
    const espfsp_server_config_t *config = instance->config;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_server(&sock, config->client_push_local.data_port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        process_receiver_connection(sock, instance);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove UDP server failed");
            break;
        }
    }

    vTaskDelete(NULL);
}
