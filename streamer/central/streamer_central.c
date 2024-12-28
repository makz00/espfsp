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

#include "mdns.h"

#include "streamer_central.h"
#include "streamer_message_types.h"
#include "streamer_central_camera_controler.h"
#include "streamer_central_camera_receiver.h"
#include "streamer_central_remote_accessor_controler.h"
#include "streamer_central_remote_accessor_sender.h"
#include "streamer_central_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT";

streamer_central_state_t *s_state = NULL;

QueueHandle_t fbQueue;

// Inteded for FB getter purpose
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

esp_err_t init_streamer_central_state(const streamer_config_t *config)
{
    s_state = (streamer_central_state_t *) malloc(sizeof(streamer_central_state_t));
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
    fbQueue = xQueueCreate(s_state->config->buffered_fbs, sizeof(streamer_message_assembly_t*));

    if (fbQueue == NULL)
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
        streamer_central_camera_data_receive_task,
        "streamer_central_camera_data_receive_task",
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
        streamer_central_camera_control_task,
        "streamer_central_camera_control_task",
        s_state->config->camera_control_task_info.stack_size,
        NULL,
        s_state->config->camera_control_task_info.task_prio,
        &s_state->camera_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t udp_streamer_handler_start_sender()
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        streamer_central_remote_accessor_data_send_task,
        "streamer_central_remote_accessor_data_send_task",
        s_state->config->data_send_task_info.stack_size,
        NULL,
        s_state->config->data_send_task_info.task_prio,
        &s_state->sender_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t udp_streamer_handler_connect_remote_accessor()
{
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        streamer_central_remote_accessor_control_task,
        "streamer_central_remote_accessor_control_task",
        s_state->config->client_control_task_info.stack_size,
        NULL,
        s_state->config->client_control_task_info.task_prio,
        &s_state->client_control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start receiver task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t start_mdns()
{
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return err;
    }

    mdns_hostname_set(s_state->config->server_mdns_name);

    return ESP_OK;
}

esp_err_t streamer_central_init(const streamer_config_t *config)
{
    esp_err_t ret;

    ret = check_config(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Config is not valid");
        return ret;
    }

    // Common part

    ret = init_streamer_central_state(config);
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

    // RECEIVER part of central

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
        ESP_LOGE(TAG, "Camera control start failed");
        return ret;
    }

    // SENDER part of central

    ret = udp_streamer_handler_start_sender();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sender start failed");
        return ret;
    }

    ret = udp_streamer_handler_connect_remote_accessor();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Remote accessor control start failed");
        return ret;
    }

    // Start broadcast server services via MDNS

    ret = start_mdns();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MDNS start failed");
        return ret;
    }

    return ret;
}

esp_err_t streamer_central_deinit(void)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED!");

    return ESP_FAIL;
}

#define FB_GET_TIMEOUT (4000 / portTICK_PERIOD_MS)

stream_fb_t *streamer_central_fb_get(void)
{
    if (s_fb == NULL)
    {
        ESP_LOGE(TAG, "Frame buffer has to be allocated first with init function");
        return NULL;
    }

    ESP_LOGI(TAG, "Taking from queue");

    BaseType_t xStatus = xQueueReceive(fbQueue, &s_ass, FB_GET_TIMEOUT);
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

    UBaseType_t elems = uxQueueMessagesWaiting(fbQueue);
    ESP_LOGI(TAG, "There is %d elems in queue now", elems);

    return s_fb;
}

void streamer_central_fb_return(stream_fb_t *fb)
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
