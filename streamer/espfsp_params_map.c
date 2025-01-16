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
    {3, ESPFSP_PARAM_MAP_CAM_FB_COUNT}};

const size_t cam_param_map_size = sizeof(cam_param_map) / sizeof(cam_param_map[0]);

espfsp_params_map_frame_entry_t frame_param_map[] = {
    {1, ESPFSP_PARAM_MAP_FRAME_PIXEL_FORMAT},
    {2, ESPFSP_PARAM_MAP_FRAME_FRAME_SIZE},
    {3, ESPFSP_PARAM_MAP_FRAME_FRAME_MAX_LEN},
    {4, ESPFSP_PARAM_MAP_FRAME_FPS}};

const size_t frame_param_map_size = sizeof(frame_param_map) / sizeof(frame_param_map[0]);

espfsp_params_map_proto_entry_t proto_param_map[] = {};

const size_t proto_param_map_size = sizeof(proto_param_map) / sizeof(proto_param_map[0]);

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
        ESP_LOGE(TAG, "Cannot map cam param id to param");
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
        ESP_LOGE(TAG, "Cannot map frame param id to param");
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_params_map_proto_param_get_param(uint16_t param_id, espfsp_params_map_proto_param_t *param)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < proto_param_map_size; i++)
    {
        if (proto_param_map[i].param_id == param_id)
        {
            *param = proto_param_map[i].param;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot proto map param id to param");
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

esp_err_t espfsp_params_map_proto_param_get_id(espfsp_params_map_proto_param_t param, uint16_t *param_id)
{
    esp_err_t ret = ESP_OK;
    bool found = false;

    for (size_t i = 0; i < proto_param_map_size; i++)
    {
        if (proto_param_map[i].param == param)
        {
            *param_id = proto_param_map[i].param_id;
            found = true;
        }
    }

    if (!found)
    {
        ESP_LOGE(TAG, "Cannot map proto param to param id");
        ret = ESP_FAIL;
    }

    return ret;
}
