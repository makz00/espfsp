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
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_message_buffer.h"
#include "espfsp_message_defs.h"

static const char *TAG = "ESPFSP_MESSAGE_BUFFER";

static bool is_assembly_producer_owner(const espfsp_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_PRODUCER_OWNED_VAL;
}

// static bool is_assembly_consumer_owner(const espfsp_message_assembly_t *assembly)
// {
//     return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_CONSUMER_OWNED_VAL;
// }

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
    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
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
    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
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

    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
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

    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
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
    receiver_buffer->config = (espfsp_receiver_buffer_config_t *) malloc(sizeof(espfsp_receiver_buffer_config_t));
    if (receiver_buffer->config == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation for config failed");
        return ESP_FAIL;
    }

    memcpy(receiver_buffer->config, config, sizeof(espfsp_receiver_buffer_config_t));

    receiver_buffer->fbs_messages_buf = (espfsp_message_assembly_t *) heap_caps_calloc(
        1,
        config->buffered_fbs * sizeof(espfsp_message_assembly_t),
        MALLOC_CAP_SPIRAM);

    if (!receiver_buffer->fbs_messages_buf)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf");
        return ESP_FAIL;
    }

    for (int i = 0; i < config->buffered_fbs; i++)
    {
        receiver_buffer->fbs_messages_buf[i].buf = (uint8_t *) heap_caps_malloc(
            config->frame_max_len * sizeof(uint8_t),
            MALLOC_CAP_SPIRAM);

        if (!receiver_buffer->fbs_messages_buf[i].buf)
        {
            ESP_LOGE(TAG, "Cannot allocate memory for fbs_messages_buf[i].buf for i=%d", i);
            return ESP_FAIL;
        }

        receiver_buffer->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    receiver_buffer->s_fb = (espfsp_fb_t *) heap_caps_malloc(sizeof(espfsp_fb_t), MALLOC_CAP_DEFAULT);
    if (receiver_buffer->s_fb == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memory for fb");
        return ESP_FAIL;
    }

    receiver_buffer->frameQueue = NULL;
    receiver_buffer->frameQueue = xQueueCreate(config->buffered_fbs, sizeof(espfsp_message_assembly_t*));
    if (receiver_buffer->frameQueue == NULL)
    {
        ESP_LOGE(TAG, "Cannot initialize message assembly queue");
        return ESP_FAIL;
    }

    receiver_buffer->buffer_locked = true;
    receiver_buffer->last_fb_get_us = 0;
    receiver_buffer->fb_get_interval_us = (uint64_t) ((1000 / config->fps) << 10);

    return ESP_OK;
}

esp_err_t espfsp_message_buffer_deinit(espfsp_receiver_buffer_t *receiver_buffer)
{
    espfsp_message_assembly_t *ass;
    while (xQueueReceive(receiver_buffer->frameQueue, &ass, 0) == pdTRUE) {}
    vQueueDelete(receiver_buffer->frameQueue);

    free(receiver_buffer->s_fb);

    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
    {
        free(receiver_buffer->fbs_messages_buf[i].buf);
    }

    free(receiver_buffer->fbs_messages_buf);
    free(receiver_buffer->config);

    return ESP_OK;
}

esp_err_t espfsp_message_buffer_clear(espfsp_receiver_buffer_t *receiver_buffer)
{
    for (int i = 0; i < receiver_buffer->config->buffered_fbs; i++)
    {
        receiver_buffer->fbs_messages_buf[i].bits = MSG_ASS_PRODUCER_OWNED_VAL | MSG_ASS_FREE_VAL;
    }

    if (xQueueReset(receiver_buffer->frameQueue) != pdPASS) {
        ESP_LOGE(TAG, "Queue reset unsuccessfully");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static bool is_buffer_locked(espfsp_receiver_buffer_t *receiver_buffer, uint32_t *timeout_ms)
{
    UBaseType_t frames_in_queue = uxQueueMessagesWaiting(receiver_buffer->frameQueue);

    if (receiver_buffer->buffer_locked)
    {
        while (frames_in_queue < receiver_buffer->config->fb_in_buffer_before_get && *timeout_ms > 0)
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            *timeout_ms -= *timeout_ms > 10 ? 10 : *timeout_ms;
            frames_in_queue = uxQueueMessagesWaiting(receiver_buffer->frameQueue);
        }

        if (frames_in_queue >= receiver_buffer->config->fb_in_buffer_before_get)
        {
            receiver_buffer->buffer_locked = false;
        }
    }
    else if (frames_in_queue == 0 && receiver_buffer->config->fb_in_buffer_before_get != 0)
    {
        receiver_buffer->buffer_locked = true;
    }

    return receiver_buffer->buffer_locked;
}

static bool is_buffer_interval_met(espfsp_receiver_buffer_t *receiver_buffer, uint32_t *timeout_ms)
{
    uint64_t current_time = esp_timer_get_time();

    while((current_time - receiver_buffer->last_fb_get_us) < receiver_buffer->fb_get_interval_us && *timeout_ms > 0)
    {
        vTaskDelay(5 / portTICK_PERIOD_MS);
        *timeout_ms -= *timeout_ms > 5 ? 5 : *timeout_ms;
        current_time = esp_timer_get_time();
    }

    return (current_time - receiver_buffer->last_fb_get_us) >= receiver_buffer->fb_get_interval_us;
}

espfsp_fb_t *espfsp_message_buffer_get_fb(espfsp_receiver_buffer_t *receiver_buffer, uint32_t timeout_ms)
{
    if (is_buffer_locked(receiver_buffer, &timeout_ms))
    {
        return NULL;
    }

    if (!is_buffer_interval_met(receiver_buffer, &timeout_ms))
    {
        return NULL;
    }

    BaseType_t xStatus = xQueueReceive(receiver_buffer->frameQueue, &receiver_buffer->s_ass, timeout_ms / portTICK_PERIOD_MS);
    if (xStatus != pdTRUE)
    {
        return NULL;
    }

    receiver_buffer->last_fb_get_us = esp_timer_get_time();

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
    if (message->len > receiver_buffer->config->frame_max_len)
    {
        ESP_LOGE(TAG, "Received message size is graeter than allocated buffer");
        return;
    }

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
