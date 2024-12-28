/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "streamer_common.h"
#include "streamer_hal.h"

typedef int esp_err_t;

typedef esp_err_t (*__streamer_start_cam)(const streamer_hal_config_t *cam_config);
typedef esp_err_t (*__streamer_stop_cam)();
typedef esp_err_t (*__streamer_send_frame)(stream_fb_t *fb);

typedef struct
{
    __streamer_start_cam start_cam;
    __streamer_stop_cam stop_cam;
    __streamer_send_frame send_frame;
} streamer_cb_t;

typedef struct
{
    streamer_task_info_t data_send_task_info;
    streamer_task_info_t control_task_info;
    streamer_connection_info_t camera_local_ports;
    streamer_connection_info_t camera_remote_ports;
    streamer_transport_t trans;
    streamer_cb_t cb;
    streamer_hal_config_t hal;
    int frame_max_len;
    int fps;
    const char *server_mdns_name;
} streamer_config_t;

/**
 * @brief Initialize the UDP streamer handler camera side application protocol
 *
 * @param config  UDP streamer handler configuration parameters
 *
 * @return ESP_OK on success
 */
esp_err_t streamer_camera_init(const streamer_config_t *config);

/**
 * @brief Deinitialize the UDP streamer handler camera
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the application protocol hasn't been initialized yet
 */
esp_err_t streamer_camera_deinit(void);
