/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

typedef enum
{
    ESPFSP_GRAB_WHEN_EMPTY,
    ESPFSP_GRAB_LATEST
} espfsp_grab_mode_t;

typedef struct
{
    espfsp_grab_mode_t cam_grab_mode;
    int cam_jpeg_quality;
    int cam_fb_count;
} espfsp_cam_config_t;
