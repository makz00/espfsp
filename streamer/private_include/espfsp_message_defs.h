/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <sys/time.h>
#include <stdint.h>

#define MESSAGE_BUFFER_SIZE 1400

#define MSG_ASS_OWNED_BIT 0x02
#define MSG_ASS_USAGE_BIT 0x01

#define MSG_ASS_PRODUCER_OWNED_VAL (0 << 1)
#define MSG_ASS_CONSUMER_OWNED_VAL (1 << 1)
#define MSG_ASS_FREE_VAL (0 << 0)
#define MSG_ASS_USED_VAL (1 << 0)

#define MESSAGE_BUFFER_SIZE 1400

typedef struct
{
    size_t len;
    size_t width;
    size_t height;
    struct timeval timestamp;
    int msg_total;
    int msg_number;
    int msg_len;
    uint8_t buf[MESSAGE_BUFFER_SIZE];
} espfsp_message_t;

typedef struct
{
    size_t len;
    size_t width;
    size_t height;
    struct timeval timestamp;
    int msg_total;
    int msg_received;
    uint8_t bits;
    uint8_t *buf;
} espfsp_message_assembly_t;
