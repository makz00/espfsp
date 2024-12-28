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

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "sys/time.h"
#include <stdint.h>

#include <stdalign.h>
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "streamer_client.h"
#include "streamer_client_central_controler.h"
#include "streamer_client_central_receiver.h"
#include "streamer_client_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT";

streamer_client_state_t *s_state = NULL;

QueueHandle_t xQueue;

static stream_fb_t *s_fb = NULL;
static streamer_message_assembly_t *s_ass = NULL;

static esp_err_t check_config()
{
    return ESP_OK;
}

esp_err_t udp_streamer_handler_buffers_init()
{

    s_state->fbs_messages_buf = (streamer_message_assembly_t *)heap_caps_aligned_calloc(
        alignof(streamer_message_assembly_t),
        1,
        s_state->config->buffered_fbs * sizeof(streamer_message_assembly_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    if (!s_state->fbs_messages_buf)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for s_state->fbs_messages");
        return ESP_FAIL;
    }

    size_t alloc_size;

    // HD resolution: 1280*720. In esp_cam it is diviced by 5. Here Resolution is diviced by 3;
    // alloc_size = 307200;

    // alloc_size = 100000;
    alloc_size = s_state->config->frame_max_len;

    // It is safe to assume that this size packet will not be fragmented
    // size_t max_udp_packet_size = 512;

    // It is 307200 / 512
    // int number_of_messages_assembly = 600;

    for (int i = 0; i < s_state->config->buffered_fbs; i++)
    {
        s_state->fbs_messages_buf[i].buf = (uint8_t *)heap_caps_malloc(
            alloc_size * sizeof(uint8_t),
            MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

        if (!s_state->fbs_messages_buf[i].buf)
        {
            ESP_LOGE(TAG, "Cannot allocate memory for s_state->fbs_messages[i].buf for i=%d", i);
            return ESP_FAIL;
        }

        s_state->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    s_fb = (stream_fb_t *)heap_caps_malloc(sizeof(stream_fb_t), MALLOC_CAP_DEFAULT);

    if (!s_fb)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fb");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t init_streamer_client_state(const streamer_config_t *config)
{
    s_state = (streamer_client_state_t *) malloc(sizeof(streamer_client_state_t));
    if (!s_state)
    {
        ESP_LOGE(TAG, "State allocation failed");
        return ESP_FAIL;
    }

    s_state->config = (streamer_config_t *) malloc(sizeof(streamer_config_t));
    if (!s_state->config)
    {
        ESP_LOGE(TAG, "State allocation failed for configuration");
        return ESP_FAIL;
    }

    memcpy(s_state->config, config, sizeof(streamer_config_t));

    return ESP_OK;
}

esp_err_t udp_streamer_handler_queue_init()
{
    xQueue = xQueueCreate(s_state->config->buffered_fbs, sizeof(streamer_message_assembly_t*));

    if (xQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize message assembly queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t udp_streamer_handler_start_receiver()
{
    BaseType_t xStatus;

    xStatus = xTaskCreatePinnedToCore(
        streamer_client_central_data_receive_task,
        "streamer_client_central_data_receive_task",
        s_state->config->data_receive_task_info.stack_size,
        NULL,
        s_state->config->data_receive_task_info.task_prio,
        &s_state->receiver_task_handle,
        1);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t udp_streamer_handler_connect_camera()
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        streamer_client_central_control_task,
        "streamer_client_central_control_task",
        s_state->config->central_control_task_info.stack_size,
        NULL,
        s_state->config->central_control_task_info.task_prio,
        &s_state->control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t streamer_client_init(const streamer_config_t *config)
{
    esp_err_t ret;

    ret = check_config(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Config is not valid");
        return ret;
    }

    ret = init_streamer_client_state(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "State initialization failed");
        return ret;
    }

    ret = udp_streamer_handler_buffers_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Buffer initialization failed");
        return ret;
    }

    ret = udp_streamer_handler_queue_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Queue initialization failed");
        return ret;
    }

    ret = udp_streamer_handler_start_receiver();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Receiver start failed");
        return ret;
    }

    ret = udp_streamer_handler_connect_camera();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Connection initialization failed");
        return ret;
    }

    return ret;
}

esp_err_t streamer_client_deinit(void)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED!");

    return ESP_FAIL;
}

#define FB_GET_TIMEOUT (4000 / portTICK_PERIOD_MS)

stream_fb_t *streamer_client_fb_get(void)
{
    if (s_fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be allocated first with init function");
        return NULL;
    }

    BaseType_t xStatus = xQueueReceive(xQueue, &s_ass, FB_GET_TIMEOUT);
    if (xStatus != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot read assembly from queue");
        return NULL;
    }

    s_fb->len = s_ass->len;
    s_fb->width = s_ass->width;
    s_fb->height = s_ass->height;
    s_fb->timestamp = s_ass->timestamp;
    s_fb->buf = (char *) s_ass->buf;

    return s_fb;
}

void streamer_client_fb_return(stream_fb_t *fb)
{
    if (s_fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be allocated first with init function");
        return;
    }

    if (s_ass == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be took first with fb get function");
        return;
    }

    s_ass->bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
}
