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

#include "espfsp_sock_op.h"
#include "espfsp_message_defs.h"

static const char *TAG = "ESPFSP_SOCK_OP";

void espfsp_set_addr(struct sockaddr_in *addr, const struct esp_ip4_addr *esp_addr, int port)
{
    addr->sin_addr.s_addr = esp_addr->addr;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
}

void espfsp_set_local_addr(struct sockaddr_in *addr, int port)
{
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
}

static int send_all_to(int sock, u_int8_t *buffer, size_t n, struct sockaddr *dest_addr)
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

static int send_all(int sock, u_int8_t *buffer, size_t n)
{
    size_t n_left = n;
    while (n_left > 0)
    {
        ssize_t bytes_sent = send(sock, buffer, n_left, 0);
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

esp_err_t espfsp_send_whole_fb_to(int sock, espfsp_fb_t *fb, struct sockaddr *dest_addr)
{
    espfsp_message_t message = {
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

        int err = send_all_to(sock, (u_int8_t *)&message, sizeof(espfsp_message_t), dest_addr);
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

esp_err_t espfsp_send_whole_fb(int sock, espfsp_fb_t *fb)
{
    espfsp_message_t message = {
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

        int err = send_all(sock, (u_int8_t *)&message, sizeof(espfsp_message_t));
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

esp_err_t espfsp_receive(int sock, char *rx_buffer, int rx_buffer_len)
{
    int accepted_error_count = 5;
    int received_bytes = 0;

    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (received_bytes < rx_buffer_len)
    {
        int len = recvfrom(sock, rx_buffer, rx_buffer_len - received_bytes, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Receiving message failed: errno %d", errno);
            if (accepted_error_count-- > 0)
            {
                continue;
            }
            else
            {
                ESP_LOGE(TAG, "Accepted error count reached. Message will not be received");
                return ESP_FAIL;
            }
        }
        else if (len == 0)
        {
            ESP_LOGE(TAG, "Sender side closed connection");
            return ESP_FAIL;
        }
        else
        {
            received_bytes += len;
            rx_buffer += len;
        }
    }

    return ESP_OK;
}

esp_err_t espfsp_tcp_accept(int *listen_sock, int *sock, struct sockaddr_in *source_addr, socklen_t *addr_len)
{
    int keep_alive = 1;
    char addr_str[128];

    *sock = accept(*listen_sock, (struct sockaddr *)source_addr, addr_len);
    if (*sock < 0)
    {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        return ESP_FAIL;
    }

    setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(int));

    inet_ntoa_r(source_addr->sin_addr, addr_str, sizeof(addr_str) - 1);

    ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
    return ESP_OK;
}

esp_err_t espfsp_create_tcp_server(int *sock, int port)
{
    struct sockaddr_in addr;
    espfsp_set_local_addr(&addr, port);

    *sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (*sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server socket created");
    int err = 0;

    int opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    err = bind(*sock, (struct sockaddr *)&addr, sizeof(addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP server socket unable to bind: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server socket bound");

    err = listen(*sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP server unable to listen: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server socket created");
    return ESP_OK;
}

esp_err_t espfsp_create_tcp_client(int *sock, int client_port, struct sockaddr_in *server_addr)
{
    struct sockaddr_in client_addr;
    espfsp_set_local_addr(&client_addr, client_port);

    *sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (*sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create TCP client socket: errno %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP client socket created");
    int err = 0;

    err = bind(*sock, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP client socket unable to bind: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP client socket bound");

    err = connect(*sock, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP client socket unable to connect: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP client socket connected");
    return ESP_OK;
}

esp_err_t espfsp_create_udp_server(int *sock, int port)
{
    struct sockaddr_in addr;
    espfsp_set_local_addr(&addr, port);

    *sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (*sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create UDP server socket: errno %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP server socket created");
    int err = 0;

    err = bind(*sock, (struct sockaddr *)&addr, sizeof(addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "UDP server socket unable to bind: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP server socket bound");
    return ESP_OK;
}

esp_err_t espfsp_create_udp_client(int *sock, int client_port, struct sockaddr_in *server_addr)
{
    struct sockaddr_in client_addr;
    espfsp_set_local_addr(&client_addr, client_port);

    *sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (*sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create UDP client socket: errno %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP client socket created");
    int err = 0;

    err = bind(*sock, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "UDP client socket unable to bind: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP client socket bound");

    err = connect(*sock, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "UDP client socket unable to connect: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP client socket connected");
    return ESP_OK;
}

esp_err_t espfsp_remove_host(int sock)
{
    int err = 0;

    err = shutdown(sock, 0);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to shutdown: errno %d", errno);
        return ESP_FAIL;
    }

    err = close(sock);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to close: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}
