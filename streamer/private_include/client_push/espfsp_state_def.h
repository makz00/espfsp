/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_client_push.h"
#include "espfsp_message_defs.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"

#define CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES 1

typedef struct {
    uint32_t session_id;
    bool active;
    bool camera_started;
} espfsp_client_push_session_data_t;

typedef struct
{
    TaskHandle_t data_task_handle;
    TaskHandle_t session_and_control_task_handle;
    espfsp_client_push_config_t *config;
    bool used;

    espfsp_fb_t sender_frame;

    espfsp_comm_proto_t comm_proto;
    espfsp_data_proto_t data_proto;

    espfsp_client_push_session_data_t session_data;
} espfsp_client_push_instance_t;

typedef struct
{
    espfsp_client_push_instance_t instances[CONFIG_ESPFSP_CLIENT_PUSH_MAX_INSTANCES];
} espfsp_client_push_state_t;
