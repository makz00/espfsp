/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_message_defs.h"
#include "espfsp_config.h"

typedef struct {
    uint32_t frame_max_len;
    uint16_t buffered_fbs;
    uint16_t fb_in_buffer_before_get;
    uint16_t fps;
} espfsp_receiver_buffer_config_t;

typedef struct {
    espfsp_receiver_buffer_config_t *config;
    QueueHandle_t frameQueue;
    espfsp_message_assembly_t *fbs_messages_buf;
    espfsp_fb_t *s_fb;
    espfsp_message_assembly_t *s_ass;
    bool buffer_locked;
    uint64_t fb_get_interval_us;
    uint64_t last_fb_get_us;
} espfsp_receiver_buffer_t;

esp_err_t espfsp_message_buffer_init(espfsp_receiver_buffer_t *receiver_buffer, const espfsp_receiver_buffer_config_t *config);
esp_err_t espfsp_message_buffer_deinit(espfsp_receiver_buffer_t *receiver_buffer);

// Allowed to use only if no other task use receive_buffer
// esp_err_t espfsp_message_buffer_clear(espfsp_receiver_buffer_t *receiver_buffer);

// Consumer interface
espfsp_fb_t *espfsp_message_buffer_get_fb(espfsp_receiver_buffer_t *receiver_buffer, uint32_t timeout_ms);
esp_err_t espfsp_message_buffer_return_fb(espfsp_receiver_buffer_t *receiver_buffer);

// Producer interface
void espfsp_message_buffer_process_message(const espfsp_message_t *message, espfsp_receiver_buffer_t *instance);
