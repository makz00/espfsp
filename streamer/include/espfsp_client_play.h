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

#define SOURCE_NAMES_MAX 3
#define SOURCE_NAME_LEN_MAX 30

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
} espfsp_client_play_config_t;

espfsp_client_play_handler_t espfsp_client_play_init(const espfsp_client_play_config_t *config);

void espfsp_client_play_deinit(espfsp_client_play_handler_t handler);

espfsp_fb_t *espfsp_client_play_get_fb(espfsp_client_play_handler_t handler, uint32_t timeout_ms);

esp_err_t espfsp_client_play_return_fb(espfsp_client_play_handler_t handler, espfsp_fb_t *fb);

esp_err_t espfsp_client_play_start_stream(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_stop_stream(espfsp_client_play_handler_t handler);

esp_err_t espfsp_client_play_reconfigure_frame(
    espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config);

esp_err_t espfsp_client_play_get_frame(
    espfsp_client_play_handler_t handler, espfsp_frame_config_t *frame_config, uint32_t timeout_ms);

esp_err_t espfsp_client_play_reconfigure_cam(
    espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config);

esp_err_t espfsp_client_play_get_cam(
    espfsp_client_play_handler_t handler, espfsp_cam_config_t *cam_config, uint32_t timeout_ms);

esp_err_t espfsp_client_play_get_sources_timeout(
    espfsp_client_play_handler_t handler,
    char sources_names_buf[SOURCE_NAMES_MAX][SOURCE_NAME_LEN_MAX],
    int *sources_names_len,
    uint32_t timeout_ms);

esp_err_t espfsp_client_play_set_source(
    espfsp_client_play_handler_t handler, const char source_name[SOURCE_NAME_LEN_MAX]);
