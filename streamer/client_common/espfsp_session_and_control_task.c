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
#include "client_common/espfsp_session_and_control_task.h"
#include "comm_proto/espfsp_comm_proto.h"

static const char *TAG = "ESPFSP_CLIENT_SESSION_AND_CONTROL_TASK";

static void handle_new_connection(espfsp_client_session_and_control_task_data_t *data, int sock)
{
    espfsp_comm_proto_t *comm_proto = data->comm_proto;

    espfsp_comm_proto_req_session_init_message_t msg = {
        .client_type = data->client_type,
    };

    esp_err_t err = espfsp_comm_proto_session_init(comm_proto, &msg);
    if (err == ESP_OK)
    {
        espfsp_comm_proto_run(comm_proto, sock);
    }
}

void espfsp_client_session_and_control_task(void *pvParameters)
{
    espfsp_client_session_and_control_task_data_t *data = (espfsp_client_session_and_control_task_data_t *) pvParameters;

    struct sockaddr_in dest_addr;
    espfsp_set_addr(&dest_addr, &data->remote_addr, data->remote_port);

    while (1) // In later phase, synchronization should be added
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_tcp_client(&sock, data->local_port, &dest_addr);
        if (ret != ESP_OK)
        {
            // Wait for 5 sec
            ESP_LOGE(TAG, "Create TCP client failed. Waiting 5 sec before retrying...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        handle_new_connection(data, sock);

        ESP_LOGI(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove TCP client failed");
            break;
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    free(data);
    vTaskDelete(NULL);
}
