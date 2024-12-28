/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "sys/time.h"

#include "streamer_hal.h"

typedef int esp_err_t;

typedef enum
{
    STREAMER_TRANSPORT_UDP,
    STREAMER_TRANSPORT_TCP,
} streamer_transport_t;

typedef struct
{
    char *buf;
    int len;
    int width;
    int height;
    streamer_pixformat_t format;
    struct timeval timestamp;
} stream_fb_t;

typedef struct
{
    int stack_size;
    int task_prio;
} streamer_task_info_t;

typedef struct
{
    int control_port;
    int data_port;
} streamer_connection_info_t;
