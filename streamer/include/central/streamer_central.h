/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "streamer_common.h"

typedef int esp_err_t;

typedef struct
{
    streamer_task_info_t data_receive_task_info;
    streamer_task_info_t data_send_task_info;
    streamer_task_info_t camera_control_task_info;
    streamer_task_info_t client_control_task_info;
    streamer_connection_info_t camera_local_ports;
    streamer_connection_info_t client_local_ports;
    streamer_transport_t trans;
    int buffered_fbs;
    int frame_max_len;
    int fps;
    const char *server_mdns_name;
} streamer_config_t;

/**
 * @brief Initialize the UDP streamer handler central side application protocol
 *
 * @param config  UDP streamer handler configuration parameters
 *
 * @return ESP_OK on success
 */
esp_err_t streamer_central_init(const streamer_config_t *config);

/**
 * @brief Deinitialize the UDP streamer handler central
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the application protocol hasn't been initialized yet
 */
esp_err_t streamer_central_deinit(void);

/**
 * @brief Obtain pointer to a frame buffer.
 *
 * @return pointer to the frame buffer
 */
stream_fb_t *streamer_central_fb_get(void);

/**
 * @brief Return the frame buffer to be reused again.
 *
 * @param fb    Pointer to the frame buffer
 */
void streamer_central_fb_return(stream_fb_t *fb);
