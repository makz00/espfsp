/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_message_buffer.h"
#include "espfsp_message_defs.h"

#define FB_GET_TIMEOUT (4000 / portTICK_PERIOD_MS)

static const char *TAG = "ESPFSP_MESSAGE_BUFFER";

static bool is_assembly_producer_owner(const espfsp_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_PRODUCER_OWNED_VAL;
}

static bool is_assembly_consumer_owner(const espfsp_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_CONSUMER_OWNED_VAL;
}

static bool is_assembly_used(const espfsp_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_USAGE_BIT) == MSG_ASS_USED_VAL;
}

static bool is_assembly_free(const espfsp_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_USAGE_BIT) == MSG_ASS_FREE_VAL;
}

static bool is_earlier(const struct timeval *lh, const struct timeval *rh)
{
    return (lh->tv_sec < rh->tv_sec) || ((lh->tv_sec == rh->tv_sec) && (lh->tv_usec < rh->tv_usec));
}

static bool is_equal(const struct timeval *lh, const struct timeval *rh)
{
    return (lh->tv_sec == rh->tv_sec) && (lh->tv_usec == rh->tv_usec);
}

static espfsp_message_assembly_t *get_assembly_with_timestamp(const struct timeval* timestamp, espfsp_receiver_buffer_t *receiver_buffer)
{
    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &receiver_buffer->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_used(cur) && is_equal(&cur->timestamp, timestamp))
        {
            return cur;
        }
    }

    return NULL;
}

static espfsp_message_assembly_t *get_free_assembly(espfsp_receiver_buffer_t *receiver_buffer)
{
    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &receiver_buffer->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_free(cur))
        {
            return cur;
        }
    }

    return NULL;
}

static espfsp_message_assembly_t *get_earliest_used_assembly(espfsp_receiver_buffer_t *receiver_buffer)
{
    espfsp_message_assembly_t *to_ret = NULL;
    struct timeval tv;

    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &receiver_buffer->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_used(cur))
        {
            tv.tv_sec = cur->timestamp.tv_sec;
            tv.tv_usec = cur->timestamp.tv_usec;
            to_ret = cur;
            break;
        }
    }

    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &receiver_buffer->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_used(cur) && is_earlier(&cur->timestamp, &tv))
        {
            tv.tv_sec = cur->timestamp.tv_sec;
            tv.tv_usec = cur->timestamp.tv_usec;
            to_ret = cur;
        }
    }

    return to_ret;
}

esp_err_t espfsp_message_buffer_init(espfsp_receiver_buffer_t *receiver_buffer, const espfsp_receiver_buffer_config_t *config)
{
    // 1. Memory allocation

    receiver_buffer->fbs_messages_buf = (espfsp_message_assembly_t *) heap_caps_aligned_calloc(
        alignof(espfsp_message_assembly_t),
        1,
        config->buffered_fbs * sizeof(espfsp_message_assembly_t),
        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    if (!receiver_buffer->fbs_messages_buf)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf");
        return ESP_FAIL;
    }

    receiver_buffer->buffered_fbs = config->buffered_fbs;

    size_t alloc_size;

    // HD resolution: 1280*720. In esp_cam it is diviced by 5. Here Resolution is diviced by 3;
    // alloc_size = 307200;

    // alloc_size = 100000;
    alloc_size = config->frame_max_len;

    // It is safe to assume that this size packet will not be fragmented
    // size_t max_udp_packet_size = 512;

    // It is 307200 / 512
    // int number_of_messages_assembly = 600;

    for (int i = 0; i < config->buffered_fbs; i++)
    {
        receiver_buffer->fbs_messages_buf[i].buf = (uint8_t *) heap_caps_malloc(
            alloc_size * sizeof(uint8_t),
            MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

        if (!receiver_buffer->fbs_messages_buf[i].buf)
        {
            ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf[i].buf for i=%d", i);
            return ESP_FAIL;
        }

        receiver_buffer->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    receiver_buffer->s_fb = (espfsp_fb_t *) heap_caps_malloc(sizeof(espfsp_fb_t), MALLOC_CAP_DEFAULT);

    if (!receiver_buffer->s_fb)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fb");
        return ESP_FAIL;
    }

    // 2. Synchronizer initiation

    receiver_buffer->frameQueue = xQueueCreate(receiver_buffer->buffered_fbs, sizeof(espfsp_message_assembly_t*));
    if (receiver_buffer->frameQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize message assembly queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_message_buffer_deinit(espfsp_receiver_buffer_t *receiver_buffer)
{
    // 1. Memory deallocation

    free(receiver_buffer->s_fb);

    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        free(receiver_buffer->fbs_messages_buf[i].buf);
    }

    free(receiver_buffer->fbs_messages_buf);

    // 2. Synchronizer deinitiation

    espfsp_message_assembly_t *ass;
    while (xQueueReceive(receiver_buffer->frameQueue, &ass, 0) == pdTRUE) {}
    vQueueDelete(receiver_buffer->frameQueue);

    return ESP_OK;
}

esp_err_t espfsp_message_buffer_clear(espfsp_receiver_buffer_t *receiver_buffer)
{
    for (int i = 0; i < receiver_buffer->buffered_fbs; i++)
    {
        receiver_buffer->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    if (xQueueReset(receiver_buffer->frameQueue) != pdPASS) {
        ESP_LOGE(TAG, "Queue reset unsuccessfully");
        return ESP_FAIL;
    }

    return ESP_OK;
}

espfsp_fb_t *espfsp_message_buffer_get_fb(espfsp_receiver_buffer_t *receiver_buffer)
{
    BaseType_t xStatus = xQueueReceive(receiver_buffer->frameQueue, &receiver_buffer->s_ass, FB_GET_TIMEOUT);
    if (xStatus != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot read assembly from queue");
        return NULL;
    }

    receiver_buffer->s_fb->len = receiver_buffer->s_ass->len;
    receiver_buffer->s_fb->width = receiver_buffer->s_ass->width;
    receiver_buffer->s_fb->height = receiver_buffer->s_ass->height;
    receiver_buffer->s_fb->timestamp = receiver_buffer->s_ass->timestamp;
    receiver_buffer->s_fb->buf = (char *) receiver_buffer->s_ass->buf;

    return receiver_buffer->s_fb;
}

esp_err_t espfsp_message_buffer_return_fb(espfsp_receiver_buffer_t *receiver_buffer)
{
    receiver_buffer->s_ass->bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    receiver_buffer->s_ass = NULL;

    return ESP_OK;
}

void espfsp_message_buffer_process_message(const espfsp_message_t *message, espfsp_receiver_buffer_t *receiver_buffer)
{
    espfsp_message_assembly_t * ass = get_assembly_with_timestamp(&message->timestamp, receiver_buffer);
    if (ass == NULL)
    {
        ass = get_free_assembly(receiver_buffer);
        if (ass == NULL && xQueueReceive(receiver_buffer->frameQueue, &ass, 0) != pdTRUE)
        {
            ass = get_earliest_used_assembly(receiver_buffer);
            if (ass == NULL)
            {
                ESP_LOGE(TAG, "Bad usage. Assemblies not returned to buffer");
                return;
            }
        }

        ass->len = message->len;
        ass->width = message->width;
        ass->height = message->height;
        ass->timestamp.tv_sec = message->timestamp.tv_sec;
        ass->timestamp.tv_usec = message->timestamp.tv_usec;
        ass->msg_total = message->msg_total;
        ass->msg_received = 0;
        ass->bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_USED_VAL;
    }

    memcpy(ass->buf + (message->msg_number * MESSAGE_BUFFER_SIZE), message->buf, message->msg_len);
    ass->msg_received += 1;

    if (ass->msg_received == ass->msg_total)
    {
        ass->bits = MSG_ASS_CONSUMER_OWNED_VAL | MSG_ASS_FREE_VAL;
        if (xQueueSend(receiver_buffer->frameQueue, &ass, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Put FB in queue FAILED");
        }
    }
}
