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

#define portTICK_PERIOD_US              ( ( TickType_t ) 1000000 / configTICK_RATE_HZ )

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

static int send_all_to(int sock, u_int8_t *buffer, size_t n, struct sockaddr_in *dest_addr)
{
    size_t n_left = n;
    while (n_left > 0)
    {
        ssize_t bytes_sent = sendto(sock, buffer, n_left, 0, (struct sockaddr *) dest_addr, sizeof(*dest_addr));
        if (bytes_sent < 0)
        {
            if (errno == ENOMEM)
            {
                continue;
            }

            ESP_LOGE(TAG, "Send to failed with errno %d", errno);
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

            ESP_LOGE(TAG, "Send to failed with errno %d", errno);
            return -1;
        }
        n_left -= bytes_sent;
        buffer += bytes_sent;
    }

    return 1;
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
            ESP_LOGE(TAG, "Error occurred during sending FB: errno %d", errno);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t espfsp_send_whole_fb_within(int sock, espfsp_fb_t *fb, uint64_t time_us)
{
    espfsp_message_t message = {
        .len = fb->len,
        .width = fb->width,
        .height = fb->height,
        .timestamp.tv_sec = fb->timestamp.tv_sec,
        .timestamp.tv_usec = fb->timestamp.tv_usec,
        .msg_total = (fb->len / MESSAGE_BUFFER_SIZE) + (fb->len % MESSAGE_BUFFER_SIZE > 0 ? 1 : 0)};

    uint32_t time_to_wait_us_per_msg = (uint32_t) (time_us / message.msg_total);
    uint32_t acc_time_to_wait_us = 0UL;

    // ESP_LOGI(TAG, "Delay per msg: %ldus", time_to_wait_us_per_msg);
    // ESP_LOGI(TAG, "Ticks len: %ldus", portTICK_PERIOD_US);

    for (size_t i = 0; i < fb->len; i += MESSAGE_BUFFER_SIZE)
    {
        int bytes_to_send = i + MESSAGE_BUFFER_SIZE <= fb->len ? MESSAGE_BUFFER_SIZE : fb->len - i;

        message.msg_number = i / MESSAGE_BUFFER_SIZE;
        message.msg_len = bytes_to_send;
        memcpy(message.buf, fb->buf + i, bytes_to_send);

        int err = send_all(sock, (u_int8_t *)&message, sizeof(espfsp_message_t));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending FB: errno %d", errno);
            return ESP_FAIL;
        }

        acc_time_to_wait_us += time_to_wait_us_per_msg;
        if (portTICK_PERIOD_US < acc_time_to_wait_us)
        {
            TickType_t ticks_to_delay = acc_time_to_wait_us / portTICK_PERIOD_US;
            acc_time_to_wait_us = acc_time_to_wait_us % portTICK_PERIOD_US;
            vTaskDelay(ticks_to_delay);
        }
    }

    return ESP_OK;
}

esp_err_t espfsp_send_whole_fb_to(int sock, espfsp_fb_t *fb, struct sockaddr_in *dest_addr)
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
            ESP_LOGE(TAG, "Error occurred during sending FB to: errno %d", errno);
            return ESP_FAIL;
        }

        // Forced delay as receiver side cannot handle a lot of Frame Buffers in short time
        const TickType_t xDelayMs = pdMS_TO_TICKS(10UL);
        vTaskDelay(xDelayMs);
    }
    // Forced delay as receiver side cannot handle a lot of Frame Buffers in short time
    const TickType_t xDelayMs = pdMS_TO_TICKS(50UL);
    vTaskDelay(xDelayMs);

    return ESP_OK;
}

esp_err_t espfsp_send(int sock, char *rx_buffer, int rx_buffer_len)
{
    int err = send_all(sock, (u_int8_t *) rx_buffer, rx_buffer_len);
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_send_state(int sock, char *rx_buffer, int rx_buffer_len, espfsp_conn_state_t *conn_state)
{
    int err = send_all(sock, (u_int8_t *) rx_buffer, rx_buffer_len);
    if (err < 0)
    {
        if (errno == EPIPE)
        {
            *conn_state = ESPFSP_CONN_STATE_CLOSED;
            return ESP_OK;
        }
        else if(errno == ECONNRESET)
        {
            *conn_state = ESPFSP_CONN_STATE_RESET;
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Probably connection terminated: errno %d", errno);
            *conn_state = ESPFSP_CONN_STATE_TERMINATED;
            return ESP_OK;
        }

        // ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        // return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_send_to(int sock, char *rx_buffer, int rx_buffer_len, struct sockaddr_in *source_addr)
{
    int err = send_all_to(sock, (u_int8_t *) rx_buffer, rx_buffer_len, source_addr);
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending to: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_receive_bytes(int sock, char *rx_buffer, int rx_buffer_len)
{
    int accepted_error_count = 5;
    int received_bytes = 0;

    while (received_bytes < rx_buffer_len)
    {
        int len = recv(sock, rx_buffer, rx_buffer_len - received_bytes, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Receiving message failed: errno %d", errno);
            if (accepted_error_count-- > 0)
            {
                ESP_LOGE(TAG, "Tries left: %d", accepted_error_count);
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

esp_err_t espfsp_receive_bytes_from(int sock, char *rx_buffer, int rx_buffer_len, struct sockaddr_in *source_addr, socklen_t *addr_len)
{
    int accepted_error_count = 5;
    int received_bytes = 0;

    while (received_bytes < rx_buffer_len)
    {
        int len = recvfrom(sock, rx_buffer, rx_buffer_len - received_bytes, 0, (struct sockaddr *)source_addr, addr_len);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Receiving message failed: errno %d", errno);
            if (accepted_error_count-- > 0)
            {
                ESP_LOGE(TAG, "Tries left: %d", accepted_error_count);
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

static esp_err_t receive_block(int sock, char *rx_buffer, int rx_buffer_len, int *received, struct timeval *timeout)
{
    *received = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    int ret = select(sock + 1, &readfds, NULL, NULL, timeout);
    if (ret > 0 && FD_ISSET(sock, &readfds)) {
        // What if not whole message will be read?
        *received = recv(sock, rx_buffer, rx_buffer_len, 0);
        if (*received < 0)
        {
            ESP_LOGE(TAG, "Receive no block error occured: errno %d", errno);
            return ESP_FAIL;
        }
        return ESP_OK;
    } else if (ret == 0) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Select failed: errno %d", errno);
        return ESP_FAIL;
    }
}

static esp_err_t receive_from_block(int sock,
    char *rx_buffer,
    int rx_buffer_len,
    int *received,
    struct timeval *timeout,
    struct sockaddr_in *source_addr,
    socklen_t *addr_len)
{
    *received = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    int ret = select(sock + 1, &readfds, NULL, NULL, timeout);
    if (ret > 0 && FD_ISSET(sock, &readfds)) {
        // What if not whole message will be read?
        *received = recvfrom(sock, rx_buffer, rx_buffer_len, 0, (struct sockaddr *)source_addr, addr_len);
        if (*received < 0)
        {
            ESP_LOGE(TAG, "Receive no block error occured: errno %d", errno);
            return ESP_FAIL;
        }
        return ESP_OK;
    } else if (ret == 0) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Select failed: errno %d", errno);
        return ESP_FAIL;
    }
}

esp_err_t espfsp_receive_block(int sock, char *rx_buffer, int rx_buffer_len, int *received, struct timeval *timeout)
{
    return receive_block(sock, rx_buffer, rx_buffer_len, received, timeout);
}

esp_err_t espfsp_receive_from_block(
    int sock,
    char *rx_buffer,
    int rx_buffer_len,
    int *received,
    struct timeval *timeout,
    struct sockaddr_in *source_addr,
    socklen_t *addr_len)
{
    return receive_from_block(sock, rx_buffer, rx_buffer_len, received, timeout, source_addr, addr_len);
}

esp_err_t espfsp_receive_no_block(int sock, char *rx_buffer, int rx_buffer_len, int *received)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return receive_block(sock, rx_buffer, rx_buffer_len, received, &timeout);
}

static esp_err_t receive_block_state(
    int sock,
    char *rx_buffer,
    int rx_buffer_len,
    int *received,
    struct timeval *timeout,
    espfsp_conn_state_t *conn_state)
{
    *received = 0;
    *conn_state = ESPFSP_CONN_STATE_GOOD;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    int ret = select(sock + 1, &readfds, NULL, NULL, timeout);
    if (ret > 0 && FD_ISSET(sock, &readfds)) {
        // What if not whole message will be read?
        *received = recv(sock, rx_buffer, rx_buffer_len, 0);
        if (*received == 0)
        {
            *conn_state = ESPFSP_CONN_STATE_CLOSED;
            return ESP_OK;
        }
        else if (*received < 0)
        {
            *received = 0;
            if (errno == ECONNRESET)
            {
                *conn_state = ESPFSP_CONN_STATE_RESET;
                return ESP_OK;
            }
            else
            {
                ESP_LOGE(TAG, "Probably connection terminated: errno %d", errno);
                *conn_state = ESPFSP_CONN_STATE_TERMINATED;
                return ESP_OK;
            }

            // ESP_LOGE(TAG, "Receive no block error occured: errno %d", errno);
            // return ESP_FAIL;
        }
        return ESP_OK;
    } else if (ret == 0) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Select failed: errno %d", errno);
        return ESP_FAIL;
    }
}

esp_err_t espfsp_receive_no_block_state(
    int sock, char *rx_buffer, int rx_buffer_len, int *received, espfsp_conn_state_t *conn_state)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return receive_block_state(sock, rx_buffer, rx_buffer_len, received, &timeout, conn_state);
}

esp_err_t espfsp_receive_from_no_block(
    int sock, char *rx_buffer, int rx_buffer_len, int *received, struct sockaddr_in *source_addr, socklen_t *addr_len)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return receive_from_block(sock, rx_buffer, rx_buffer_len, received, &timeout, source_addr, addr_len);
}

esp_err_t espfsp_connect(int sock, struct sockaddr_in *source_addr)
{
    esp_err_t ret = ESP_OK;

    ret = connect(sock, (struct sockaddr *) source_addr, sizeof(struct sockaddr_in));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
    }

    return ret;
}

esp_err_t espfsp_tcp_accept(int listen_sock, int *sock, struct sockaddr_in *source_addr, socklen_t *addr_len)
{
    char addr_str[128];

    *sock = accept(listen_sock, (struct sockaddr *) source_addr, addr_len);
    if (*sock < 0)
    {
        if (errno == EWOULDBLOCK)
        {
            *sock = -1;
            return ESP_OK;
        }

        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        return ESP_FAIL;
    }

    int keep_alive_opt = 1;

    setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_opt, sizeof(keep_alive_opt));

    int tcp_keepalive_opt = 20000; // TCP keepalive period in milliseconds
    // int tcp_keepidle_opt = 1; // same as TCP_KEEPALIVE, but the value is in seconds
    int tcp_keepintvl_opt = 4; // the interval between keepalive probes in seconds
    int tcp_keepcnt_opt = 5; // number of keepalive probes before timing out

    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPALIVE, &tcp_keepalive_opt, sizeof(tcp_keepalive_opt));
    // setsockopt(*sock, IPPROTO_TCP, TCP_KEEPIDLE, &tcp_keepidle_opt, sizeof(tcp_keepidle_opt));
    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPINTVL, &tcp_keepintvl_opt, sizeof(tcp_keepintvl_opt));
    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPCNT, &tcp_keepcnt_opt, sizeof(tcp_keepcnt_opt));

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

    int flags = fcntl(*sock, F_GETFL);
    if (fcntl(*sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        ESP_LOGE(TAG, "Unable to set socket non blocking: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    int reuse_addr_opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt));

    err = bind(*sock, (struct sockaddr *)&addr, sizeof(addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP server socket unable to bind: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server socket bound");

    err = listen(*sock, 3);
    if (err != 0)
    {
        ESP_LOGE(TAG, "TCP server unable to listen: errno %d", errno);
        close(*sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server socket listen");
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

    int opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int keep_alive_opt = 1;

    setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_opt, sizeof(keep_alive_opt));

    int tcp_keepalive_opt = 20000; // TCP keepalive period in milliseconds
    // int tcp_keepidle_opt = 1; // same as TCP_KEEPALIVE, but the value is in seconds
    int tcp_keepintvl_opt = 4; // the interval between keepalive probes in seconds
    int tcp_keepcnt_opt = 5; // number of keepalive probes before timing out

    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPALIVE, &tcp_keepalive_opt, sizeof(tcp_keepalive_opt));
    // setsockopt(*sock, IPPROTO_TCP, TCP_KEEPIDLE, &tcp_keepidle_opt, sizeof(tcp_keepidle_opt));
    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPINTVL, &tcp_keepintvl_opt, sizeof(tcp_keepintvl_opt));
    setsockopt(*sock, IPPROTO_TCP, TCP_KEEPCNT, &tcp_keepcnt_opt, sizeof(tcp_keepcnt_opt));

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

    int opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    int opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    err = close(sock);
    if (err != 0 && errno != ENOTCONN)
    {
        ESP_LOGE(TAG, "Socket unable to close: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_remove_udp_host(int sock)
{
    int err = 0;

    err = close(sock);
    if (err != 0 && errno != ENOTCONN)
    {
        ESP_LOGE(TAG, "Socket unable to close: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}
