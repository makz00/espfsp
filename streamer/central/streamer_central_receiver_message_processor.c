/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

#include "esp_err.h"
#include "esp_log.h"

#include "central/streamer_central_receiver_message_processor.h"
#include "central/streamer_central_types.h"

static const char *TAG = "UDP_STREAMER_COMPONENT_RECEIVER_MESSAGE_PROCESSOR";

extern streamer_central_state_t *s_state;

extern QueueHandle_t fbQueue;

static bool is_assembly_producer_owner(const streamer_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_PRODUCER_OWNED_VAL;
}

static bool is_assembly_consumer_owner(const streamer_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_OWNED_BIT) == MSG_ASS_CONSUMER_OWNED_VAL;
}

static bool is_assembly_used(const streamer_message_assembly_t *assembly)
{
    return (assembly->bits & MSG_ASS_USAGE_BIT) == MSG_ASS_USED_VAL;
}

static bool is_assembly_free(const streamer_message_assembly_t *assembly)
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

static streamer_message_assembly_t *get_assembly_with_timestamp(const struct timeval* timestamp)
{
    for (int i = 0; i < s_state->config->buffered_fbs; i++)
    {
        streamer_message_assembly_t *cur = &s_state->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_used(cur) && is_equal(&cur->timestamp, timestamp))
        {
            return cur;
        }
    }

    return NULL;
}

static streamer_message_assembly_t *get_free_assembly()
{
    for (int i = 0; i < s_state->config->buffered_fbs; i++)
    {
        streamer_message_assembly_t *cur = &s_state->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_free(cur))
        {
            return cur;
        }
    }

    return NULL;
}

void streamer_central_process_message(const streamer_message_t *message)
{
    streamer_message_assembly_t * ass = get_assembly_with_timestamp(&message->timestamp);
    if (ass == NULL)
    {
        ass = get_free_assembly();
        if (ass == NULL && xQueueReceive(fbQueue, &ass, 0) != pdTRUE)
        {
            return;
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
        ESP_LOGI(TAG, "Put FB in queue");

        ass->bits = MSG_ASS_CONSUMER_OWNED_VAL | MSG_ASS_FREE_VAL;
        if (xQueueSend(fbQueue, &ass, 0) != pdPASS)
        {
            ESP_LOGI(TAG, "Put FB in queue FAILED");
        }

        UBaseType_t elems = uxQueueMessagesWaiting(fbQueue);
        ESP_LOGI(TAG, "There is %d elems in queue", elems);
    }
}
