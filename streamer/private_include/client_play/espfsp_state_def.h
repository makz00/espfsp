/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_client_play.h"
#include "espfsp_message_defs.h"
#include "espfsp_message_buffer.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"

#define CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES 1

typedef struct {
    SemaphoreHandle_t mutex;
    uint32_t session_id;
    bool active;
    bool stream_started;
} espfsp_client_play_session_data_t;

typedef struct {
    char sources_names_buf[SOURCE_NAMES_MAX][SOURCE_NAME_LEN_MAX];
    int sources_names_len;
} espfsp_get_sources_data_t;

typedef struct {
    uint16_t param_id;
    uint32_t param_value;
} espfsp_get_param_data_t;

typedef struct
{
    TaskHandle_t data_task_handle;
    TaskHandle_t session_and_control_task_handle;
    espfsp_client_play_config_t *config;
    bool used;

    espfsp_receiver_buffer_t receiver_buffer;

    espfsp_comm_proto_t comm_proto;
    espfsp_data_proto_t data_proto;

    QueueHandle_t get_sources_data_queue;
    QueueHandle_t get_frame_config_data_queue;
    QueueHandle_t get_cam_config_data_queue;

    espfsp_client_play_session_data_t session_data;
} espfsp_client_play_instance_t;

typedef struct
{
    espfsp_client_play_instance_t instances[CONFIG_ESPFSP_CLIENT_PLAY_MAX_INSTANCES];
} espfsp_client_play_state_t;
