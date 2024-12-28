/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include <freertos/task.h>

#include "streamer_message_types.h"
#include "central/streamer_central.h"

typedef struct
{
    TaskHandle_t receiver_task_handle;
    TaskHandle_t sender_task_handle;
    TaskHandle_t camera_control_task_handle;
    TaskHandle_t client_control_task_handle;
    streamer_central_config_t *config;

    streamer_message_assembly_t *fbs_messages_buf;
} streamer_central_state_t;
