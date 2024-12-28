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
#include "streamer_camera_controler.h"
#include "streamer_camera_types.h"

static const char *TAG = "STREAMER_CAMERA_CONRTOLER";

static const char *PAYLOAD_HELLO = "HELLO";
static const char *PAYLOAD_READY = "READY";

extern streamer_camera_state_t *s_state;

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

void streamer_camera_control_task(void *pvParameters)
{
    assert(s_state != NULL);
    assert(s_state->config != NULL);

    streamer_config_t * config = s_state->config;

    struct sockaddr_in control_addr;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(config->camera_local_ports.control_port);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = s_state->server_addr.addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config->camera_remote_ports.control_port);

    while (1)
    {
        int err;

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "Socket created");

        err = bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            continue;
        }

        ESP_LOGI(TAG, "Connecting to %s:%d", s_state->server_addr_str, config->camera_remote_ports.control_port);

        err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            continue;
        }

        ESP_LOGI(TAG, "Successfully connected");

        process_control_connection(sock);

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}
