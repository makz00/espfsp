/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <sys/time.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif.h"

#include "espfsp_config.h"

typedef void * espfsp_client_push_handler_t;

typedef struct
{
    espfsp_task_info_t data_task_info;
    espfsp_task_info_t session_and_control_task_info;

    espfsp_connection_info_t local;
    espfsp_connection_info_t remote;
    espfsp_transport_t data_transport;
    struct esp_ip4_addr remote_addr;

    espfsp_client_push_cb_t cb;
    espfsp_frame_config_t frame_config;
    espfsp_cam_config_t cam_config;
} espfsp_client_push_config_t;

espfsp_client_push_handler_t espfsp_client_push_init(const espfsp_client_push_config_t *config);

void espfsp_client_push_deinit(espfsp_client_push_handler_t handler);
