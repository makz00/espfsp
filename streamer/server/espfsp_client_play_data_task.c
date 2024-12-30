/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_netif.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_sock_op.h"
#include "server/espfsp_client_play_data_task.h"
#include "server/espfsp_server_state_def.h"

static const char *TAG = "ESPFSP_SERVER_CLIENT_PLAY_DATA_TASK";

static const char *BULLET_PAYLOAD = "BULLET";

static esp_err_t wait_for_nat_bullet_from_accessor(int sock, struct sockaddr_storage *accessor_addr, socklen_t *socklen)
{
    char rx_buffer[128];

    while (1)
    {
        int len = recvfrom(sock, rx_buffer, 127, 0, (struct sockaddr *)accessor_addr, socklen);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
            return ESP_FAIL;
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
            return ESP_FAIL;
        }

        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

        if (len != strlen(BULLET_PAYLOAD))
        {
            ESP_LOGE(TAG, "Not send whole bullet payload");
            continue;
        }

        if (strcmp(BULLET_PAYLOAD, rx_buffer) == 0)
        {
            break;
        }

        ESP_LOGE(TAG, "Bullet payload did not match");
    }

    return ESP_OK;
}

#define FB_GET_TIMEOUT (4000 / portTICK_PERIOD_MS)

static espfsp_fb_t *get_fb(espfsp_server_instance_t *instance)
{
    if (instance->s_fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be allocated first with init function");
        return NULL;
    }

    BaseType_t xStatus = xQueueReceive(instance->frameQueue, instance->s_ass, FB_GET_TIMEOUT);
    if (xStatus != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot read assembly from queue");
        return NULL;
    }

    instance->s_fb->len = instance->s_ass->len;
    instance->s_fb->width = instance->s_ass->width;
    instance->s_fb->height = instance->s_ass->height;
    instance->s_fb->timestamp = instance->s_ass->timestamp;
    instance->s_fb->buf = (char *) instance->s_ass->buf;

    UBaseType_t elems = uxQueueMessagesWaiting(instance->frameQueue);
    ESP_LOGI(TAG, "There is %d elems in queue now", elems);

    return instance->s_fb;
}

static void fb_return(espfsp_server_instance_t *instance, espfsp_fb_t *fb)
{
    if (instance->s_fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be allocated first with init function");
        return;
    }

    if (instance->s_ass == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be took first with fb get function");
        return;
    }

    instance->s_ass->bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
}

static void process_sender_connection(int sock, espfsp_server_instance_t *instance)
{
    esp_err_t ret = 0;

    struct sockaddr_storage accessor_addr;
    socklen_t socklen = sizeof(accessor_addr);

    ret = wait_for_nat_bullet_from_accessor(sock, &accessor_addr, &socklen);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Cannot receive NAT bullet from accessor");
        return;
    }

    ESP_LOGI(TAG, "Received correct bullet");

    while (1)
    {
        espfsp_fb_t *fb = streamer_central_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "FB capture Failed");
            continue;
        }

        ret = espfsp_udp_send_whole_fb_to(sock, fb, (struct sockaddr *)&accessor_addr);

        streamer_central_fb_return(fb);

        if (ret < 0)
        {
            ESP_LOGE(TAG, "Cannot send message. End sender process");
            break;
        }

        ESP_LOGI(TAG, "Message send");
    }
}

void espfsp_server_client_play_data_task(void *pvParameters)
{
    espfsp_server_instance_t *instance = (espfsp_server_instance_t *) pvParameters;

    assert(instance != NULL);
    assert(instance->config != NULL);

    const espfsp_server_config_t *config = instance->config;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_udp_server(&sock, config->client_play_local.data_port);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create UDP server failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        process_sender_connection(sock, instance);

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
