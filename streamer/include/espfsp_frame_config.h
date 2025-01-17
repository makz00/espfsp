/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t frame_max_len;
    uint16_t buffered_fbs;
    uint16_t fb_in_buffer_before_get;
    uint16_t fps;
} espfsp_frame_config_t;
