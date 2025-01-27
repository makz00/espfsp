/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "stdbool.h"

#include "espfsp_params_map.h"

static const char *TAG = "ESPFSP_PARAMS_MAP";

espfsp_params_map_cam_entry_t cam_param_map[] = {
    {1, ESPFSP_PARAM_MAP_CAM_GRAB_MODE},
    {2, ESPFSP_PARAM_MAP_CAM_JPEG_QUALITY},
    {3, ESPFSP_PARAM_MAP_CAM_FB_COUNT},
    {4, ESPFSP_PARAM_MAP_CAM_PIXEL_FORMAT},
    {5, ESPFSP_PARAM_MAP_CAM_FRAME_SIZE}};

const size_t cam_param_map_size = sizeof(cam_param_map) / sizeof(cam_param_map[0]);

espfsp_params_map_frame_entry_t frame_param_map[] = {
    {1, ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN},
    {2, ESPFSP_PARAM_MAP_FRAME_BUFFERED_FBS},
    {3, ESPFSP_PARAM_MAP_FRAME_FB_IN_BUFFER_BEFORE_GET},
    {4, ESPFSP_PARAM_MAP_FRAME_FPS}};

const size_t frame_param_map_size = sizeof(frame_param_map) / sizeof(frame_param_map[0]);

esp_err_t espfsp_params_map_cam_param_get_param(uint16_t param_id, espfsp_params_map_cam_param_t *param)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < cam_param_map_size; i++)
    {
        if (cam_param_map[i].param_id == param_id)
        {
            *param = cam_param_map[i].param;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot map cam param id to param. Received id: %d", param_id);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_params_map_frame_param_get_param(uint16_t param_id, espfsp_params_map_frame_param_t *param)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < frame_param_map_size; i++)
    {
        if (frame_param_map[i].param_id == param_id)
        {
            *param = frame_param_map[i].param;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot map frame param id to param. Received id: %d", param_id);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_params_map_cam_param_get_id(espfsp_params_map_cam_param_t param, uint16_t *param_id)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < cam_param_map_size; i++)
    {
        if (cam_param_map[i].param == param)
        {
            *param_id = cam_param_map[i].param_id;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot map cam param to param id");
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_params_map_frame_param_get_id(espfsp_params_map_frame_param_t param, uint16_t *param_id)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < frame_param_map_size; i++)
    {
        if (frame_param_map[i].param == param)
        {
            *param_id = frame_param_map[i].param_id;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot map frame param to param id");
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_params_map_set_frame_config(espfsp_frame_config_t *frame_config, uint16_t param_id, uint32_t value)
{
    esp_err_t ret = ESP_OK;

    espfsp_params_map_frame_param_t param;
    ret = espfsp_params_map_frame_param_get_param(param_id, &param);
    if (ret == ESP_OK)
    {
        switch (param)
        {
        case ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN:
            frame_config->frame_max_len = (uint32_t) value;
            ESP_LOGI(TAG, "Set frame_max_len: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_BUFFERED_FBS:
            frame_config->buffered_fbs = (uint16_t) value;
            ESP_LOGI(TAG, "Set buffered_fbs: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FB_IN_BUFFER_BEFORE_GET:
            frame_config->fb_in_buffer_before_get = (uint16_t) value;
            ESP_LOGI(TAG, "Set fb_in_buffer_before_get: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FPS:
            frame_config->fps = (uint16_t) value;
            ESP_LOGI(TAG, "Set fps: %ld", value);
            break;
        default:
            ESP_LOGE(TAG, "Not handled frame parameter");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_params_map_set_cam_config(espfsp_cam_config_t *cam_config, uint16_t param_id, uint32_t value)
{
    esp_err_t ret = ESP_OK;

    espfsp_params_map_cam_param_t param;
    ret = espfsp_params_map_cam_param_get_param(param_id, &param);
    if (ret == ESP_OK)
    {
        switch (param)
        {
        case ESPFSP_PARAM_MAP_CAM_GRAB_MODE:
            cam_config->cam_grab_mode = (espfsp_grab_mode_t) value;
            ESP_LOGI(TAG, "Set cam_grab_mode: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_CAM_JPEG_QUALITY:
            cam_config->cam_jpeg_quality = (int) value;
            ESP_LOGI(TAG, "Set cam_jpeg_quality: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_CAM_FB_COUNT:
            cam_config->cam_fb_count = (int) value;
            ESP_LOGI(TAG, "Set cam_fb_count: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_CAM_PIXEL_FORMAT:
            cam_config->cam_pixel_format = (espfsp_pixformat_t) value;
            ESP_LOGI(TAG, "Set cam_pixel_format: %ld", value);
            break;
        case ESPFSP_PARAM_MAP_CAM_FRAME_SIZE:
            cam_config->cam_frame_size = (espfsp_framesize_t) value;
            ESP_LOGI(TAG, "Set cam_frame_size: %ld", value);
            break;
        default:
            ESP_LOGE(TAG, "Not handled cam parameter");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_params_map_get_frame_config_param_val(espfsp_frame_config_t *frame_config, uint16_t param_id, uint32_t *value)
{
    esp_err_t ret = ESP_OK;

    espfsp_params_map_frame_param_t param;
    ret = espfsp_params_map_frame_param_get_param(param_id, &param);
    if (ret == ESP_OK)
    {
        switch (param)
        {
        case ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN:
            *value = (uint32_t) frame_config->frame_max_len;
            ESP_LOGI(TAG, "Read frame_max_len: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_BUFFERED_FBS:
            *value = (uint32_t) frame_config->buffered_fbs;
            ESP_LOGI(TAG, "Read buffered_fbs: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FB_IN_BUFFER_BEFORE_GET:
            *value = (uint32_t) frame_config->fb_in_buffer_before_get;
            ESP_LOGI(TAG, "Read fb_in_buffer_before_get: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_FRAME_FPS:
            *value = (uint32_t) frame_config->fps;
            ESP_LOGI(TAG, "Read fps: %ld", *value);
            break;
        default:
            ESP_LOGE(TAG, "Not handled frame parameter");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t espfsp_params_map_get_cam_config_param_val(espfsp_cam_config_t *cam_config, uint16_t param_id, uint32_t *value)
{
    esp_err_t ret = ESP_OK;

    espfsp_params_map_cam_param_t param;
    ret = espfsp_params_map_cam_param_get_param(param_id, &param);
    if (ret == ESP_OK)
    {
        switch (param)
        {
        case ESPFSP_PARAM_MAP_CAM_GRAB_MODE:
            *value = (uint32_t) cam_config->cam_grab_mode;
            ESP_LOGI(TAG, "Read cam_grab_mode: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_CAM_JPEG_QUALITY:
            *value = (uint32_t) cam_config->cam_jpeg_quality;
            ESP_LOGI(TAG, "Read cam_jpeg_quality: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_CAM_FB_COUNT:
            *value = (uint32_t) cam_config->cam_fb_count;
            ESP_LOGI(TAG, "Read cam_fb_count: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_CAM_PIXEL_FORMAT:
            *value = (uint32_t) cam_config->cam_pixel_format;
            ESP_LOGI(TAG, "Read cam_pixel_format: %ld", *value);
            break;
        case ESPFSP_PARAM_MAP_CAM_FRAME_SIZE:
            *value = (uint32_t) cam_config->cam_frame_size;
            ESP_LOGI(TAG, "Read cam_frame_size: %ld", *value);
            break;
        default:
            ESP_LOGE(TAG, "Not handled cam parameter");
            ret = ESP_FAIL;
        }
    }

    return ret;
}
