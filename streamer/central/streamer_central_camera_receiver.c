/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

/*
 * ASSUMPTIONS BEG --------------------------------------------------------------------------------
 * - One format of pictures supported - HD compressed with JPEG
 * - Constant size for one Frame Buffer, so allocation can take place before messagas arrived
 * ASSUMPTIONS END --------------------------------------------------------------------------------
 *
 * TODO BEG ---------------------------------------------------------------------------------------
 * - Configuration options to add in 'menuconfig'/Kconfig file
 * TODO END ---------------------------------------------------------------------------------------
 */

#include <arpa/inet.h>
#include "esp_netif.h"
#include <netdb.h>
#include <sys/socket.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "streamer_central.h"
#include "streamer_central_camera_receiver.h"
#include "streamer_central_camera_receiver_message_processor.h"
#include "streamer_central_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT_RECEIVER";

extern streamer_central_state_t *s_state;

static int receive_whole_message(int sock, char *rx_buffer, int rx_buffer_len)
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
                return -1;
            }
        }
        else if (len == 0)
        {
            ESP_LOGE(TAG, "Sender side closed connection");
            return -1;
        }
        else
        {
            received_bytes += len;
            rx_buffer += len;
        }
    }

    return 1;
}

// static void punch_hole_in_nat(int sock)
// {
//     uint8_t bullet = 0;
//     while (sendto(sock, &bullet, sizeof(bullet), 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) > 0) {}
// }

static void process_receiver_connection(int sock)
{
    char rx_buffer[sizeof(streamer_message_t)];

    while (1)
    {
        int ret = receive_whole_message(sock, rx_buffer, sizeof(streamer_message_t));
        if (ret > 0)
        {
            // ESP_LOGI(
            //     TAG,
            //     "Received msg part: %d/%d for timestamp: sek: %lld, usek: %ld",
            //     ((streamer_message_t *)rx_buffer)->msg_number,
            //     ((streamer_message_t *)rx_buffer)->msg_total,
            //     ((streamer_message_t *)rx_buffer)->timestamp.tv_sec,
            //     ((streamer_message_t *)rx_buffer)->timestamp.tv_usec);

            process_message((streamer_message_t *)rx_buffer);
        }
        else
        {
            ESP_LOGE(TAG, "Cannot receive message. End receiver process");
            break;
        }
    }
}

void streamer_central_camera_data_receive_task(void *pvParameters)
{
    assert(s_state != NULL);
    assert(s_state->config != NULL);

    streamer_config_t *config = s_state->config;

    struct sockaddr_in receiver_addr;
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(config->camera_local_ports.data_port);

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

        err = bind(sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            continue;
        }

        ESP_LOGI(TAG, "Socket bound, port %d", config->camera_local_ports.data_port);

        process_receiver_connection(sock);

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}
