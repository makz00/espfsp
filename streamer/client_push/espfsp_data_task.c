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

#include "espfsp_message_defs.h"
#include "espfsp_client_push.h"
#include "espfsp_sock_op.h"
#include "client_push/espfsp_data_task.h"
#include "client_push/espfsp_state_def.h"

static const char *TAG = "ESPFSP_CLIENT_PUSH_DATA_TASK";

static void process_sender_connection(int sock, const espfsp_client_push_config_t *config)
{
    esp_err_t ret;
    espfsp_fb_t fb;

    ret = config->cb.start_cam(&config->cam_config, &config->frame_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Start camera failed");
        return;
    }

    fb.buf = (char *) malloc(config->frame_config.frame_max_len);
    if (fb.buf == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for frame buffer");
        return;
    }

    while (1)
    {
        ret = config->cb.send_frame(&fb);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Send frame buffer failed");
            continue;
        }

        ret = espfsp_send_whole_fb(sock, &fb);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot send message. End sender process");
            break;
        }
    }

    free(fb.buf);
}

void espfsp_client_push_data_task(void *pvParameters)
{
    espfsp_client_push_instance_t *instance = (espfsp_client_push_instance_t *) pvParameters;
    const espfsp_client_push_config_t *config = instance->config;

    struct sockaddr_in dest_addr;
    espfsp_set_addr(&dest_addr, &config->remote_addr, config->remote.data_port);

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

        process_sender_connection(sock, config);

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
