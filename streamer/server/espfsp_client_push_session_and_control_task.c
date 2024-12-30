/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "server/espfsp_client_push_session_and_control_task.h"
#include "server/espfsp_server_state_def.h"

static const char *TAG = "ESPFSP_SERVER_CLIENT_PUSH_SESSION_AND_CONTROL_TASK";

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

void espfsp_server_client_push_session_and_control_task(void *pvParameters)
{
    const espfsp_server_instance_t *instance = (espfsp_server_instance_t *) pvParameters;

    assert(instance != NULL);
    assert(instance->config != NULL);

    const espfsp_server_config_t *config = instance->config;

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int listen_sock = 0;

        ret = espfsp_create_tcp_server(&listen_sock, config->client_push_local.control_port);
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

            process_control_connection(sock);

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
