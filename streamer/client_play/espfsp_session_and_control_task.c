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

#include "espfsp_client_play.h"
#include "client_play/espfsp_session_and_control_task.h"
#include "client_play/espfsp_state_def.h"

static const char *TAG = "ESPFSP_CLIENT_PLAY_SESSION_AND_CONTROL_TASK";

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
    int ret = send(sock, PAYLOAD_HELLO, strlen(PAYLOAD_HELLO), 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return CONTROL_STATE_RET_FAIL;
    }

    if (ret != strlen(PAYLOAD_HELLO))
    {
        ESP_LOGE(TAG, "Not send whole message");
        return CONTROL_STATE_RET_FAIL;
    }

    return CONTROL_STATE_RET_OK;
}

static control_state_ret_t process_control_handle_ready(int sock)
{
    int ret = send(sock, PAYLOAD_READY, strlen(PAYLOAD_READY), 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return CONTROL_STATE_RET_FAIL;
    }

    if (ret != strlen(PAYLOAD_READY))
    {
        ESP_LOGE(TAG, "Not send whole message");
        return CONTROL_STATE_RET_FAIL;
    }

    return CONTROL_STATE_RET_OK;
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

void espfsp_client_play_session_and_control_task(void *pvParameters)
{
    espfsp_client_play_instance_t *instance = (espfsp_client_play_instance_t *) pvParameters;
    const espfsp_client_play_config_t *config = instance->config;

    struct esp_ip4_addr remote_addr;
    remote_addr.addr = inet_addr(config->host_ip);

    struct sockaddr_in dest_addr;
    espfsp_set_addr(&dest_addr, &remote_addr, config->remote.control_port);

    while (1)
    {
        esp_err_t ret = ESP_OK;
        int sock = 0;

        ret = espfsp_create_tcp_client(&sock, config->local.control_port, &dest_addr);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Create TCP client failed");
            continue;
        }

        ESP_LOGI(TAG, "Start processing messages");

        process_control_connection(sock);

        ESP_LOGE(TAG, "Shut down socket and restart...");

        ret = espfsp_remove_host(sock);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Remove TCP client failed");
            break;
        }
    }

    vTaskDelete(NULL);
}
