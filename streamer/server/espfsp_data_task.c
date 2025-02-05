/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"

#include "espfsp_sock_op.h"
#include "server/espfsp_data_task.h"
#include "data_proto/espfsp_data_proto.h"

static const char *TAG = "ESPFSP_SERVER_DATA_TASK";

static void handle_new_connection(espfsp_server_data_task_data_t *data, int sock)
{
    espfsp_data_proto_t *data_proto = data->data_proto;

    espfsp_data_proto_run(data_proto, sock);
}

void espfsp_server_data_task(void *pvParameters)
{
    espfsp_server_data_task_data_t *data = (espfsp_server_data_task_data_t *) pvParameters;

    while (1) // In later phase, synchronization should be added
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_server(&sock, data->server_port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        handle_new_connection(data, sock);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_udp_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove UDP server failed");
            break;
        }
    }

    free(data);
    vTaskDelete(NULL);
}
