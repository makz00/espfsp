/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

/*
 * ASSUMPTIONS BEG --------------------------------------------------------------------------------
 * ASSUMPTIONS END --------------------------------------------------------------------------------
 *
 * TODO BEG ---------------------------------------------------------------------------------------
 * - Configuration options to add in 'menuconfig'/Kconfig file
 * TODO END ---------------------------------------------------------------------------------------
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "sys/time.h"
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mdns.h"

#include "mdns_helper.h"
#include "streamer_camera.h"
#include "streamer_camera_controler.h"
#include "streamer_camera_sender.h"
#include "streamer_camera_types.h"

static const char *TAG = "UDP_STREAMER_CAMERA_COMPONENT";

streamer_camera_state_t *s_state = NULL;

static esp_err_t check_config()
{
    return ESP_OK;
}

static esp_err_t init_streamer_camera_state(const streamer_config_t *config)
{
    s_state = (streamer_camera_state_t *) malloc(sizeof(streamer_camera_state_t));
    if (!s_state)
    {
        ESP_LOGE(TAG, "Allocation of 's_state' failed");
        return ESP_FAIL;
    }

    s_state->config = (streamer_config_t *) malloc(sizeof(streamer_config_t));
    if (!s_state->config)
    {
        ESP_LOGE(TAG, "Allocation of 's_state->config' failed");
        return ESP_FAIL;
    }

    memcpy(s_state->config, config, sizeof(streamer_config_t));

    return ESP_OK;
}

esp_err_t wait_for_central()
{
    struct esp_ip4_addr *server_addr = &s_state->server_addr;
    char *server_addr_str = s_state->server_addr_str;

    server_addr->addr = 0;

    return wait_for_server(s_state->config->server_mdns_name, server_addr, server_addr_str, sizeof(s_state->server_addr_str));
}

esp_err_t start_mdns()
{
    esp_err_t ret;

    ret = mdns_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MDNS Init failed");
        return ret;
    }

    ret = wait_for_central();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wait for central failed");
        return ret;
    }

    return ESP_OK;
}

static esp_err_t init_camera()
{
    esp_err_t ret;

    ret = s_state->config->cb.start_cam(&s_state->config->hal);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t streamer_camera_data_send_start()
{
    BaseType_t xStatus = xTaskCreate(
        streamer_camera_data_send_task,
        "streamer_camera_data_send_task",
        s_state->config->data_send_task_info.stack_size,
        NULL,
        s_state->config->data_send_task_info.task_prio,
        &s_state->sender_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start sender task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t streamer_camera_control_start()
{
    BaseType_t xStatus = xTaskCreate(
        streamer_camera_control_task,
        "streamer_camera_control_task",
        s_state->config->control_task_info.stack_size,
        NULL,
        s_state->config->control_task_info.task_prio,
        &s_state->control_task_handle);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Could not start control task!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t streamer_camera_init(const streamer_config_t *config)
{
    esp_err_t ret;

    ret = check_config(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Config is not valid");
        return ret;
    }

    ret = init_streamer_camera_state(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "State initialization failed");
        return ret;
    }

    ret = start_mdns();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MDNS start failed");
        return ret;
    }

    ret = init_camera();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera initialization failed");
        return ESP_FAIL;
    }

    ret = streamer_camera_data_send_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Sender has not been created successfully");
        return ret;
    }

    ret = streamer_camera_control_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Controler has not been created successfully");
        return ret;
    }

    return ret;
}

esp_err_t streamer_camera_deinit(void)
{
    ESP_LOGE(TAG, "NOT IMPLEMENTED!");

    return ESP_FAIL;
}
