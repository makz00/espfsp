/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "espfsp_cam_config.h"
#include "espfsp_frame_config.h"

typedef enum
{
    ESPFSP_PARAM_MAP_CAM_GRAB_MODE,
    ESPFSP_PARAM_MAP_CAM_JPEG_QUALITY,
    ESPFSP_PARAM_MAP_CAM_FB_COUNT,
    ESPFSP_PARAM_MAP_CAM_PIXEL_FORMAT,
    ESPFSP_PARAM_MAP_CAM_FRAME_SIZE,
} espfsp_params_map_cam_param_t;

typedef enum
{
    ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN,
    ESPFSP_PARAM_MAP_FRAME_BUFFERED_FBS,
    ESPFSP_PARAM_MAP_FRAME_FB_IN_BUFFER_BEFORE_GET,
    ESPFSP_PARAM_MAP_FRAME_FPS,
} espfsp_params_map_frame_param_t;

typedef struct
{
    uint16_t param_id;
    espfsp_params_map_cam_param_t param;
} espfsp_params_map_cam_entry_t;

typedef struct
{
    uint16_t param_id;
    espfsp_params_map_frame_param_t param;
} espfsp_params_map_frame_entry_t;

extern espfsp_params_map_cam_entry_t cam_param_map[];
extern const size_t cam_param_map_size;

extern espfsp_params_map_frame_entry_t frame_param_map[];
extern const size_t frame_param_map_size;

esp_err_t espfsp_params_map_cam_param_get_param(uint16_t param_id, espfsp_params_map_cam_param_t *param);
esp_err_t espfsp_params_map_frame_param_get_param(uint16_t param_id, espfsp_params_map_frame_param_t *param);

esp_err_t espfsp_params_map_cam_param_get_id(espfsp_params_map_cam_param_t param, uint16_t *param_id);
esp_err_t espfsp_params_map_frame_param_get_id(espfsp_params_map_frame_param_t param, uint16_t *param_id);

esp_err_t espfsp_params_map_set_frame_config(espfsp_frame_config_t *frame_config, uint16_t param_id, uint32_t value);
esp_err_t espfsp_params_map_set_cam_config(espfsp_cam_config_t *cam_config, uint16_t param_id, uint32_t value);

esp_err_t espfsp_params_map_get_frame_config_param_val(espfsp_frame_config_t *frame_config, uint16_t param_id, uint32_t *value);
esp_err_t espfsp_params_map_get_cam_config_param_val(espfsp_cam_config_t *cam_config, uint16_t param_id, uint32_t *value);
