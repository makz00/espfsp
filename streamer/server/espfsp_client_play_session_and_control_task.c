/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>
#include <errno.h>

#include <esp_err.h>
#include <esp_log.h>

#include "esp_netif.h"
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_sock_op.h"
#include "server/espfsp_client_play_session_and_control_task.h"
#include "server/espfsp_state_def.h"
#include "comm_proto/espfsp_comm_proto.h"

static const char *TAG = "ESPFSP_SERVER_CLIENT_PLAY_SESSION_AND_CONTROL_TASK";

static void handle_new_client_play_connection(espfsp_server_instance_t *instance, int sock)
{
    // Spawn ping task
    espfsp_comm_proto_run(&instance->client_play_comm_proto, sock);
}

void espfsp_server_client_play_session_and_control_task(void *pvParameters)
{
    const espfsp_server_instance_t *instance = (espfsp_server_instance_t *) pvParameters;
    const espfsp_server_config_t *config = instance->config;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int listen_sock = 0;

        ret = espfsp_create_tcp_server(&listen_sock, config->client_play_local.control_port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create TCP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing connections");

        while (1)
        {
            int sock = 0;
            struct sockaddr_in source_addr;
            socklen_t addr_len = sizeof(source_addr);

            ret = espfsp_tcp_accept(&listen_sock, &sock, &source_addr, &addr_len);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Accept TCP connection failed");
                continue;
            }

            ESP_LOGI(TAG, "Start processing messages");

            handle_new_client_play_connection(instance, sock);

            ESP_LOGE(TAG, "Shut down socket and restart...");

            ret = espfsp_remove_host(sock);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Remove UDP server failed");
                break;
            }
        }
    }

    vTaskDelete(NULL);
}
