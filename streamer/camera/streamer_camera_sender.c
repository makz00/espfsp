/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

/*
 * ASSUMPTIONS BEG --------------------------------------------------------------------------------
 * ASSUMPTIONS END --------------------------------------------------------------------------------
 *
 * TODO BEG ---------------------------------------------------------------------------------------
 * - Configuration options to add in 'menuconfig'/Kconfig file
 * TODO END ---------------------------------------------------------------------------------------
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

#include "mdns_helper.h"
#include "streamer_camera.h"
#include "streamer_message_types.h"
#include "streamer_camera_sender.h"
#include "streamer_camera_types.h"

static const char *TAG = "STREAMER_CAMERA_SENDER";

extern streamer_camera_state_t *s_state;

static int send_all(int sock, u_int8_t *buffer, size_t n, struct sockaddr_in *dest_addr)
{
    size_t n_left = n;
    while (n_left > 0)
    {
        ssize_t bytes_sent = sendto(sock, buffer, n_left, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr));
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

static esp_err_t send_whole_message(int sock, stream_fb_t *fb, struct sockaddr_in *dest_addr)
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
            return ESP_FAIL;
        }

        const TickType_t xDelayMs = pdMS_TO_TICKS(10UL);
        vTaskDelay(xDelayMs);
    }
    // Forced delay as receiver side cannot handle a lot of Frame Buffers in short time
    const TickType_t xDelayMs = pdMS_TO_TICKS(50UL);
    vTaskDelay(xDelayMs);

    return ESP_OK;
}

static void process_sender_connection(int sock, struct sockaddr_in *dest_addr)
{
    streamer_config_t * config = s_state->config;
    stream_fb_t fb;

    fb.buf = (char *) malloc(config->frame_max_len);
    if (fb.buf == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate mamory for frame buffer purpose");
        return;
    }

    while (1)
    {
        esp_err_t ret;

        ret = config->cb.send_frame(&fb);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Frame buffer capture failed. Skip and try capture next next frame");
            continue;
        }

        ret = send_whole_message(sock, &fb, dest_addr);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot send message. End sender process");
            break;
        }
    }
}

void streamer_camera_data_send_task(void *pvParameters)
{
    assert(s_state != NULL);
    assert(s_state->config != NULL);

    streamer_config_t *config = s_state->config;

    struct sockaddr_in sender_addr;
    sender_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_port = htons(config->camera_local_ports.data_port);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = s_state->server_addr.addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config->camera_remote_ports.data_port);

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

        ESP_LOGI(TAG, "Sending to %s:%d", s_state->server_addr_str, config->camera_remote_ports.data_port);

        process_sender_connection(sock, &dest_addr);

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}
