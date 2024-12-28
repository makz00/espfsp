/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include <arpa/inet.h>
#include "esp_netif.h"
#include <netdb.h>
#include <sys/socket.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "central/streamer_central.h"
#include "central/streamer_central_sender.h"
#include "central/streamer_central_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT_CENTRAL_SENDER";

static const char *BULLET_PAYLOAD = "BULLET";

extern streamer_central_state_t *s_state;

static int wait_for_nat_bullet_from_accessor(int sock, struct sockaddr_storage *accessor_addr, socklen_t *socklen)
{
    char rx_buffer[128];

    while (1)
    {
        int len = recvfrom(sock, rx_buffer, 127, 0, (struct sockaddr *)accessor_addr, socklen);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
            return -1;
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
            return -1;
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

    return 1;
}

static int send_all(int sock, u_int8_t *buffer, size_t n, struct sockaddr *dest_addr)
{
    size_t n_left = n;
    while (n_left > 0)
    {
        ssize_t bytes_sent = sendto(sock, buffer, n_left, 0, dest_addr, sizeof(*dest_addr));
        if (bytes_sent < 0)
        {
            if (errno == ENOMEM)
            {
                continue;
            }

            return -1;
        }
        n_left -= bytes_sent;
        buffer += bytes_sent;
    }

    return 1;
}

static int send_whole_message(int sock, stream_fb_t *fb, struct sockaddr *dest_addr)
{
    streamer_message_t message = {
        .len = fb->len,
        .width = fb->width,
        .height = fb->height,
        .timestamp.tv_sec = fb->timestamp.tv_sec,
        .timestamp.tv_usec = fb->timestamp.tv_usec,
        .msg_total = (fb->len / MESSAGE_BUFFER_SIZE) + (fb->len % MESSAGE_BUFFER_SIZE > 0 ? 1 : 0)};

    for (size_t i = 0; i < fb->len; i += MESSAGE_BUFFER_SIZE)
    {
        int bytes_to_send = i + MESSAGE_BUFFER_SIZE <= fb->len ? MESSAGE_BUFFER_SIZE : fb->len - i;

        message.msg_number = i / MESSAGE_BUFFER_SIZE;
        message.msg_len = bytes_to_send;
        memcpy(message.buf, fb->buf + i, bytes_to_send);

        int err = send_all(sock, (u_int8_t *)&message, sizeof(streamer_message_t), dest_addr);
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return -1;
        }

        // const TickType_t xDelayMs = pdMS_TO_TICKS(3UL);
        // vTaskDelay(xDelayMs);
    }
    // Forced delay as receiver side cannot handle a lot of Frame Buffers in short time
    // const TickType_t xDelayMs = pdMS_TO_TICKS(40UL);
    // vTaskDelay(xDelayMs);

    return 1;
}

static void process_sender_connection(int sock)
{
    int ret = 0;

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
        stream_fb_t *fb = streamer_central_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "FB capture Failed");
            continue;
        }

        int ret = send_whole_message(sock, fb, (struct sockaddr *)&accessor_addr);

        streamer_central_fb_return(fb);

        if (ret < 0)
        {
            ESP_LOGE(TAG, "Cannot send message. End sender process");
            break;
        }

        ESP_LOGI(TAG, "Message send");
    }
}

void streamer_central_sender_task(void *pvParameters)
{
    assert(s_state != NULL);
    assert(s_state->config != NULL);

    streamer_central_config_t *config = s_state->config;

    struct sockaddr_in sender_addr;
    sender_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_port = htons(config->client_local_ports.data_port);

    while (1)
    {
        int err;

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "Socket created");

        err = bind(sock, (struct sockaddr *)&sender_addr, sizeof(sender_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            continue;
        }

        ESP_LOGI(TAG, "Socket bound, port %d", config->client_local_ports.data_port);

        process_sender_connection(sock);

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}
