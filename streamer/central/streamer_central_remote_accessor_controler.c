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

#include "streamer_central.h"
#include "streamer_central_remote_accessor_controler.h"
#include "streamer_central_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT_CENTRAL_REMOTE_ACESSOR";

extern streamer_central_state_t *s_state;

static const char *PAYLOAD_HELLO = "HELLO";
static const char *PAYLOAD_READY = "READY";

typedef enum
{
    CONTROL_STATE_HELLO,
    CONTROL_STATE_READY,
    CONTROL_STATE_IDLE,
} control_state_t;

typedef enum
{
    CONTROL_STATE_RET_OK,
    CONTROL_STATE_RET_FAIL,
    CONTROL_STATE_RET_ERR,
} control_state_ret_t;

static control_state_ret_t process_control_handle_hello(int sock)
{
    char rx_buffer[128];

    int ret = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        return CONTROL_STATE_RET_FAIL;
    }
    else if (ret == 0)
    {
        ESP_LOGW(TAG, "Connection closed");
        return CONTROL_STATE_RET_ERR;
    }

    if (ret != strlen(PAYLOAD_HELLO))
    {
        ESP_LOGE(TAG, "Not send whole message");
        return CONTROL_STATE_RET_FAIL;
    }

    rx_buffer[ret] = 0;
    ESP_LOGI(TAG, "Received %d bytes: %s", ret, rx_buffer);

    if (strcmp(PAYLOAD_HELLO, rx_buffer) == 0)
    {
        return CONTROL_STATE_RET_OK;
    }

    ESP_LOGE(TAG, "Hello message did not match");
    return CONTROL_STATE_RET_FAIL;
}

static control_state_ret_t process_control_handle_ready(int sock)
{
    char rx_buffer[128];

    int ret = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        return CONTROL_STATE_RET_FAIL;
    }
    else if (ret == 0)
    {
        ESP_LOGW(TAG, "Connection closed");
        return CONTROL_STATE_RET_ERR;
    }

    if (ret != strlen(PAYLOAD_READY))
    {
        ESP_LOGE(TAG, "Not send whole message");
        return CONTROL_STATE_RET_FAIL;
    }

    rx_buffer[ret] = 0;
    ESP_LOGI(TAG, "Received %d bytes: %s", ret, rx_buffer);

    if (strcmp(PAYLOAD_READY, rx_buffer) == 0)
    {
        return CONTROL_STATE_RET_OK;
    }

    ESP_LOGE(TAG, "Hello message did not match");
    return CONTROL_STATE_RET_FAIL;
}

static void process_control_connection(int sock)
{
    control_state_ret_t ret;
    control_state_t state;

    state = CONTROL_STATE_HELLO;

    while (1)
    {
        switch (state)
        {
        case CONTROL_STATE_HELLO:
            ret = process_control_handle_hello(sock);

            if (ret == CONTROL_STATE_RET_OK)
            {
                ESP_LOGI(TAG, "State HELLO reached");
                state = CONTROL_STATE_READY;
            }
            break;

        case CONTROL_STATE_READY:
            ret = process_control_handle_ready(sock);

            if (ret == CONTROL_STATE_RET_OK)
            {
                ESP_LOGI(TAG, "State READY reached");
                state = CONTROL_STATE_IDLE;
            }
            else if (ret == CONTROL_STATE_RET_FAIL)
            {
                state = CONTROL_STATE_HELLO;
            }
            break;

        case CONTROL_STATE_IDLE:
            ESP_LOGI(TAG, "State IDLE reached");
            vTaskSuspend(NULL);
            break;
        }

        if (ret == CONTROL_STATE_RET_ERR)
        {
            ESP_LOGE(TAG, "Control protocol end with critical error");
            break;
        }
    }
}

void streamer_central_remote_accessor_control_task(void *pvParameters)
{
    assert(s_state != NULL);
    assert(s_state->config != NULL);

    streamer_config_t *config = s_state->config;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config->client_local_ports.control_port);

    int keep_alive = 1;
    char addr_str[128];

    while (1)
    {
        int err;

        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        ESP_LOGI(TAG, "Socket created");

        err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            continue;
        }

        ESP_LOGI(TAG, "Socket bound, port %d", config->client_local_ports.control_port);

        err = listen(listen_sock, 1);
        if (err != 0)
        {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            close(listen_sock);
            continue;
        }

        while (1)
        {
            ESP_LOGI(TAG, "Socket listening");

            struct sockaddr_in source_addr;
            socklen_t addr_len = sizeof(source_addr);

            ESP_LOGI(TAG, "Waiting for host to control connection ...");

            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0)
            {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                continue;
            }

            setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(int));

            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

            process_control_connection(sock);

            shutdown(sock, 0);
            close(sock);

            ESP_LOGI(TAG, "Socket closed");
        }
    }

    vTaskDelete(NULL);
}
