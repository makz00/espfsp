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
#include "client_common/espfsp_data_task.h"
#include "data_proto/espfsp_data_proto.h"

static const char *TAG = "ESPFSP_CLIENT_DATA_TASK";

static void handle_new_connection(espfsp_client_data_task_data_t *data, int sock)
{
    espfsp_data_proto_t *data_proto = data->data_proto;

    espfsp_data_proto_run(data_proto, sock);
}

void espfsp_client_data_task(void *pvParameters)
{
    espfsp_client_data_task_data_t *data = (espfsp_client_data_task_data_t *) pvParameters;

    struct sockaddr_in dest_addr;
    espfsp_set_addr(&dest_addr, &data->remote_addr, data->remote_port);

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_client(&sock, data->local_port, &dest_addr);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP client failed");
            continue;
        }

        ESP_LOGI(TAG, "Start process connection");

        handle_new_connection(data, sock);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove UDP client failed");
            break;
        }
    }

    free(data);
    vTaskDelete(NULL);
}
