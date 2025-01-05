/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <esp_err.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"

#include "espfsp_sock_op.h"
#include "server/espfsp_session_and_control_task.h"
#include "comm_proto/espfsp_comm_proto.h"

static const char *TAG = "ESPFSP_SERVER_SESSION_AND_CONTROL_TASK";

static void handle_new_connection(espfsp_session_and_control_task_data_t *data, int sock)
{
    espfsp_comm_proto_t *comm_proto = data->comm_proto;

    espfsp_comm_proto_run(comm_proto, sock);
}

void espfsp_server_session_and_control_task(void *pvParameters)
{
    espfsp_session_and_control_task_data_t *data = (espfsp_session_and_control_task_data_t *) pvParameters;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int listen_sock = 0;

        ret = espfsp_create_tcp_server(&listen_sock, data->port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create TCP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Process incoming connections");

        while (1)
        {
            int sock = 0;
            struct sockaddr_in source_addr;
            socklen_t addr_len = sizeof(source_addr);

            ret = espfsp_tcp_accept(listen_sock, &sock, &source_addr, &addr_len);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Accept TCP connection failed");
                continue;
            }

            ESP_LOGI(TAG, "Processing connection");

            handle_new_connection(data, sock);

            ESP_LOGE(TAG, "Shut down socket and restart...");

            ret = espfsp_remove_host(sock);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Remove UDP server failed");
                break;
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}
