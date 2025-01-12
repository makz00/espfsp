/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "stdbool.h"

#include "espfsp_params_map.h"

static const char *TAG = "ESPFSP_PARAMS_MAP";

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
