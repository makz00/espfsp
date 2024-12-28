/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include <freertos/task.h>

#include "esp_netif_ip_addr.h"

#include "camera/streamer_camera.h"

typedef struct
{
    TaskHandle_t sender_task_handle;
    TaskHandle_t control_task_handle;
    streamer_camera_config_t *config;

    struct esp_ip4_addr server_addr;
    char server_addr_str[128];
} streamer_camera_state_t;
