/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include <freertos/task.h>

#include "streamer_client.h"
#include "streamer_message_types.h"

typedef struct
{
    TaskHandle_t receiver_task_handle;
    TaskHandle_t control_task_handle;
    streamer_config_t *config;

    streamer_message_assembly_t *fbs_messages_buf;
} streamer_client_state_t;
