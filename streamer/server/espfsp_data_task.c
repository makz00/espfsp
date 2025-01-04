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
#include "espfsp_server.h"
#include "server/espfsp_data_task.h"
#include "server/espfsp_state_def.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"

static const char *TAG = "ESPFSP_SERVER_DATA_TASK";

static void handle_new_connection(espfsp_data_task_data_t *data, int sock)
{
    espfsp_comm_proto_t *comm_proto = data->comm_proto;
    espfsp_comm_proto_t *data_proto = data->data_proto;

    espfsp_data_proto_run(data_proto, sock, comm_proto);
}

void espfsp_server_client_push_data_task(void *pvParameters)
{
    espfsp_data_task_data_t *data = (espfsp_data_task_data_t *) pvParameters;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_server(&sock, data->port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        handle_new_connection(data, sock);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove UDP server failed");
            break;
        }
    }

    free(data);
    vTaskDelete(NULL);
}
