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

typedef void * espfsp_client_play_handler_t;

typedef struct
{
    espfsp_task_info_t data_task_info;
    espfsp_task_info_t session_and_control_task_info;

    espfsp_connection_info_t local;
    espfsp_connection_info_t remote;
    espfsp_transport_t data_transport;
    struct esp_ip4_addr remote_addr;

    espfsp_frame_config_t frame_config;
    espfsp_cam_config_t cam_config;
    uint16_t buffered_fbs;
} espfsp_client_play_config_t;

espfsp_client_play_handler_t espfsp_client_play_init(const espfsp_client_play_config_t *config);

void espfsp_client_play_deinit(espfsp_client_play_handler_t handler);

espfsp_fb_t *espfsp_client_play_get_fb(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_return_fb(espfsp_client_play_handler_t handler, espfsp_fb_t *fb);

esp_err_t espfsp_client_play_start_stream(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_stop_stream(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_reconfigure_frame(espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config);

esp_err_t espfsp_client_play_reconfigure_cam(espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config);

esp_err_t espfsp_client_play_reconfigure_protocol_params(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_get_sources(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_set_source(espfsp_client_play_handler_t handler);
