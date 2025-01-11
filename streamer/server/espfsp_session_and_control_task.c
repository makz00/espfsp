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
#include "server/espfsp_session_manager.h"
#include "comm_proto/espfsp_comm_proto.h"

static const char *TAG = "ESPFSP_SERVER_SESSION_AND_CONTROL_TASK";

static void handle_new_connection_task(espfsp_server_session_and_control_task_data_t *data, int sock)
{
    esp_err_t err = ESP_OK;
    espfsp_session_manager_t *session_manager = data->session_manager;
    espfsp_comm_proto_t *comm_proto = NULL;

    ESP_LOGI(TAG, "Processing connection");

    err = espfsp_session_manager_take(session_manager);
    if (err == ESP_OK)
    {
        err = espfsp_session_manager_get_comm_proto(session_manager, data->session_type, &comm_proto);
        espfsp_session_manager_release(session_manager);
    }

    if (err == ESP_OK && comm_proto != NULL)
    {
        espfsp_comm_proto_run(comm_proto, sock);

        err = espfsp_session_manager_take(session_manager);
        if (err == ESP_OK)
        {
            err = espfsp_session_manager_return_comm_proto(session_manager, comm_proto);
            espfsp_session_manager_release(session_manager);
        }
    }

    ESP_LOGE(TAG, "Shut down socket and restart...");

    esp_err_t ret = espfsp_remove_host(sock);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Remove UDP server failed");
    }

    vTaskDelete(NULL);
}

void espfsp_server_session_and_control_task(void *pvParameters)
{
    espfsp_server_session_and_control_task_data_t *data = (espfsp_server_session_and_control_task_data_t *) pvParameters;

    while (1)  // In later phase, synchronization should be added
    {
        esp_err_t ret = ESP_OK;
        int listen_sock = 0;

        ret = espfsp_create_tcp_server(&listen_sock, data->server_port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create TCP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Process incoming connections");

        while (1) // In later phase, synchronization should be added
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

            // In order to safely deinitialize, task handler should be kept and freed
            BaseType_t xStatus = xTaskCreate(
                handle_new_connection_task,
                "handle_new_connection_task",
                data->connection_task_info.stack_size,
                (void *) data,
                data->connection_task_info.task_prio,
                NULL);

            if (xStatus != pdPASS)
            {
                ESP_LOGE(TAG, "Task create for new connection failed");
            }
        }

        ret = espfsp_remove_host(listen_sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove TCP server failed");
        }
    }

    free(data);
    vTaskDelete(NULL);
}
