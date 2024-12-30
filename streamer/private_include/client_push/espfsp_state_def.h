/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_client_push.h"
#include "espfsp_message_defs.h"

#define CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES 1

typedef struct
{
    TaskHandle_t data_task_handle;
    TaskHandle_t session_and_control_task_handle;
    espfsp_client_push_config_t *config;
    bool used;
} espfsp_client_push_instance_t;

typedef struct
{
    espfsp_client_push_instance_t instances[CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES];
} espfsp_client_push_state_t;
