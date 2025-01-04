/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include <stdint.h>
#include <stddef.h>

#include "data_proto/espfsp_data_recv_proto.h"
#include "data_proto/espfsp_data_send_proto.h"
#include "data_proto/espfsp_data_proto.h"

static const char *TAG = "ESPFSP_DATA_PROTOCOL";

esp_err_t espfsp_data_proto_init(espfsp_data_proto_t *data_proto, espfsp_data_proto_config_t *config)
{
    esp_err_t ret = ESP_OK;

    data_proto->config = (espfsp_data_proto_config_t *) malloc(sizeof(espfsp_data_proto_config_t));
    if (!data_proto->config)
    {
        ESP_LOGE(TAG, "Cannot initialize memory for config");
        return ESP_FAIL;
    }

    memcpy(data_proto->config, config, sizeof(espfsp_data_proto_config_t));
}

esp_err_t espfsp_data_proto_deinit(espfsp_data_proto_t *data_proto)
{
    free(data_proto->config);
    return ESP_OK;
}

static esp_err_t handle_data_proto(espfsp_data_proto_t *data_proto, int sock)
{
    esp_err_t ret = ESP_OK;

    switch (data_proto->config->type)
    {
    case ESPFSP_DATA_PROTO_TYPE_SEND:

        ret = espfsp_data_proto_handle_send(data_proto, sock);
        break;

    case ESPFSP_DATA_PROTO_TYPE_RECV:

        ret = espfsp_data_proto_handle_recv(data_proto, sock);
        break;

    default:

        ESP_LOGE(TAG, "Data protocol type not handled");
        return ESP_FAIL;
    }

    return ret;
}

static void change_state_base_ret(espfsp_data_proto_t *data_proto, espfsp_data_proto_state_t next_state, esp_err_t ret_val)
{
    if (ret_val != ESP_OK)
        data_proto->state = ESPFSP_COMM_PROTO_STATE_ERROR;
    else
        data_proto->state = next_state;
}

esp_err_t espfsp_data_proto_run(espfsp_data_proto_t *data_proto, int sock, espfsp_comm_proto_t *comm_proto)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Start data handling");

    data_proto->state = ESPFSP_DATA_PROTO_STATE_LOOP;
    data_proto->last_traffic = NO_SIGNAL;
    data_proto->en = 1;

    while (data_proto->en)
    {
        switch (data_proto->state)
        {
        case ESPFSP_DATA_PROTO_STATE_LOOP:

            ret = handle_data_proto(data_proto, sock);
            change_state_base_ret(data_proto, ESPFSP_DATA_PROTO_STATE_CONTROL, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_CONTROL:

            // Check if sending parameters are OK
            change_state_base_ret(data_proto, ESPFSP_DATA_PROTO_STATE_LOOP, ret);
            break;

        case ESPFSP_DATA_PROTO_STATE_ERROR:

            ESP_LOGE(TAG, "Data protocol failed");
            // send_notif_to_control_task
            return ESP_FAIL;

        default:

            ESP_LOGE(TAG, "Data protocol state not handled");
            data_proto->state = ESPFSP_DATA_PROTO_STATE_ERROR;
            break;
        }
    }

    ESP_LOGI(TAG, "Stop data handling");
    return ESP_OK;
}

esp_err_t espfsp_data_proto_stop(espfsp_data_proto_t *data_proto)
{
    // Safe asynchronious write as other task, after start, only reads this variable, so it is atomic. Races still can
    // occure.
    data_proto->en = 0;
    return ESP_OK;
}
