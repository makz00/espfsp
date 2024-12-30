/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

#include "esp_err.h"
#include "esp_log.h"

#include "server/espfsp_server_state_def.h"
#include "server/espfsp_client_push_data_message_processor.h"
#include "espfsp_message_defs.h"

static const char *TAG = "ESPFSP_SERVER_CLIENT_PUSH_DATA_MESSAGE_PROCESSOR";

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

static espfsp_message_assembly_t *get_assembly_with_timestamp(const struct timeval* timestamp, espfsp_server_instance_t *instance)
{
    for (int i = 0; i < instance->config->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &instance->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_used(cur) && is_equal(&cur->timestamp, timestamp))
        {
            return cur;
        }
    }

    return NULL;
}

static espfsp_message_assembly_t *get_free_assembly(espfsp_server_instance_t *instance)
{
    for (int i = 0; i < instance->config->buffered_fbs; i++)
    {
        espfsp_message_assembly_t *cur = &instance->fbs_messages_buf[i];

        if (is_assembly_producer_owner(cur) && is_assembly_free(cur))
        {
            return cur;
        }
    }

    return NULL;
}

void espfsp_server_client_push_process_message(const espfsp_message_t *message, espfsp_server_instance_t *instance)
{
    espfsp_message_assembly_t * ass = get_assembly_with_timestamp(&message->timestamp, instance);
    if (ass == NULL)
    {
        ass = get_free_assembly(instance);
        if (ass == NULL && xQueueReceive(instance->frameQueue, &ass, 0) != pdTRUE)
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
        if (xQueueSend(instance->frameQueue, &ass, 0) != pdPASS)
        {
            ESP_LOGI(TAG, "Put FB in queue FAILED");
        }

        UBaseType_t elems = uxQueueMessagesWaiting(instance->frameQueue);
        ESP_LOGI(TAG, "There is %d elems in queue", elems);
    }
}
