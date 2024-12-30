/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_client_play.h"
#include "espfsp_message_defs.h"

#define CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES 1

typedef struct
{
    TaskHandle_t data_task_handle;
    TaskHandle_t session_and_control_task_handle;
    espfsp_client_play_config_t *config;
    espfsp_message_assembly_t *fbs_messages_buf;
    bool used;

    QueueHandle_t frameQueue;
    espfsp_fb_t *s_fb;
    espfsp_message_assembly_t *s_ass;
} espfsp_client_play_instance_t;

typedef struct
{
    espfsp_client_play_instance_t instances[CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES];
} espfsp_client_play_state_t;
