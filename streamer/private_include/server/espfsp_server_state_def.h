/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_message_defs.h"

#define CONFIG_ESPFSP_SERVER_MAX_INSTANCES 1
#define CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS 1
#define CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS 1

typedef struct
{
    TaskHandle_t data_task_handle;
    TaskHandle_t session_and_control_task_handle;
} espfsp_server_client_handlers_t;

typedef struct
{
    espfsp_server_client_handlers_t client_push_handlers[CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS];
    espfsp_server_client_handlers_t client_play_handlers[CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS];
    espfsp_server_config_t *config;
    espfsp_message_assembly_t *fbs_messages_buf;
    bool used;

    QueueHandle_t frameQueue;
    espfsp_fb_t *s_fb;
    espfsp_message_assembly_t *s_ass;
} espfsp_server_instance_t;

typedef struct
{
    espfsp_server_instance_t instances[CONFIG_ESPFSP_SERVER_MAX_INSTANCES];
} espfsp_server_state_t;
