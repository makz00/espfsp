/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <sys/time.h>
#include <stdint.h>

#include "esp_err.h"

#include "espfsp_config.h"

typedef void * espfsp_server_handler_t;

typedef struct
{
    espfsp_task_info_t client_push_data_task_info;
    espfsp_task_info_t client_play_data_task_info;
    espfsp_task_info_t client_push_session_and_control_task_info;
    espfsp_task_info_t client_play_session_and_control_task_info;

    espfsp_connection_info_t client_push_local;
    espfsp_connection_info_t client_play_local;
    espfsp_transport_t client_push_data_transport;
    espfsp_transport_t client_play_data_transport;

    espfsp_frame_config_t frame_config;
    espfsp_cam_config_t cam_config;
} espfsp_server_config_t;

espfsp_server_handler_t espfsp_server_init(const espfsp_server_config_t *config);

void espfsp_server_deinit(espfsp_server_handler_t handler);
