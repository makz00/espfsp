/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "sys/time.h"

#include "espfsp_cam_config.h"
#include "espfsp_frame_config.h"

typedef int esp_err_t;

typedef struct
{
    int len;
    int width;
    int height;
    espfsp_pixformat_t format;
    struct timeval timestamp;
    char *buf;
} espfsp_fb_t;

typedef struct
{
    int stack_size;
    int task_prio;
} espfsp_task_info_t;

typedef struct
{
    int control_port;
    int data_port;
} espfsp_connection_info_t;

typedef enum
{
    ESPFSP_TRANSPORT_UDP,
    ESPFSP_TRANSPORT_TCP,
} espfsp_transport_t;

typedef esp_err_t (*__espfsp_start_cam)(const espfsp_cam_config_t *cam_config, const espfsp_frame_config_t *frame_config);
typedef esp_err_t (*__espfsp_stop_cam)();
typedef esp_err_t (*__espfsp_send_frame)(espfsp_fb_t *fb);
typedef esp_err_t (*__espfsp_send_reconf_cam)(const espfsp_cam_config_t *cam_config);
typedef esp_err_t (*__espfsp_send_reconf_frame)(const espfsp_frame_config_t *frame_config);

typedef struct
{
    __espfsp_start_cam start_cam;
    __espfsp_stop_cam stop_cam;
    __espfsp_send_frame send_frame;
    __espfsp_send_reconf_cam reconf_cam;
    __espfsp_send_reconf_frame reconf_frame;
} espfsp_client_push_cb_t;
