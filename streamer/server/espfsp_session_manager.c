/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "string.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stddef.h>

#include "server/espfsp_session_manager.h"
#include "server/espfsp_comm_proto_conf.h"

#define UNACTIVE_SESSION_ID -1

static const char *TAG = "ESPFSP_SERVER_SESSION_MANAGER";

esp_err_t espfsp_session_manager_init(
    espfsp_session_manager_t *session_manager, espfsp_server_session_manager_config_t *config)
{
    esp_err_t ret = ESP_OK;

    session_manager->config = (espfsp_server_session_manager_config_t *) malloc(
        sizeof(espfsp_server_session_manager_config_t));
    if (session_manager->config == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed for configuration");
        return ESP_FAIL;
    }

    memcpy(session_manager->config, config, sizeof(espfsp_server_session_manager_config_t));

    session_manager->client_push_session_data = (espfsp_server_session_manager_data_t *) malloc(
        sizeof(espfsp_server_session_manager_data_t) * config->client_push_comm_protos_count);
    if (session_manager->client_push_session_data == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed for client push session data");
        return ESP_FAIL;
    }

    session_manager->client_play_session_data = (espfsp_server_session_manager_data_t *) malloc(
        sizeof(espfsp_server_session_manager_data_t) * config->client_play_comm_protos_count);
    if (session_manager->client_play_session_data == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed for client play session data");
        return ESP_FAIL;
    }

    for (int i = 0; i < config->client_push_comm_protos_count; i++)
    {
        espfsp_server_session_manager_data_t *data = &session_manager->client_push_session_data[i];

        data->comm_proto = &config->client_push_comm_protos[i];
        data->type = ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH;
        data->active = false;
    }

    for (int i = 0; i < config->client_play_comm_protos_count; i++)
    {
        espfsp_server_session_manager_data_t *data = &session_manager->client_play_session_data[i];

        data->comm_proto = &config->client_play_comm_protos[i];
        data->type = ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY;
        data->active = false;
    }

    session_manager->client_push_session_data_count = config->client_push_comm_protos_count;
    session_manager->client_play_session_data_count = config->client_play_comm_protos_count;
    session_manager->primary_client_push_session_data = NULL;
    session_manager->primary_client_play_session_data = NULL;

    session_manager->mutex = NULL;
    session_manager->mutex = xSemaphoreCreateBinary();
    if (session_manager->mutex == NULL)
    {
        ESP_LOGE(TAG, "Cannot init semaphore");
        return ESP_FAIL;
    }

    if (xSemaphoreGive(session_manager->mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give after init semaphore");
        return ESP_FAIL;
    }

    return ret;
}

esp_err_t espfsp_session_manager_deinit(espfsp_session_manager_t *session_manager)
{
    free(session_manager->client_push_session_data);
    free(session_manager->client_play_session_data);
    free(session_manager->config);

    vSemaphoreDelete(session_manager->mutex);

    return ESP_OK;
}

static espfsp_server_session_manager_data_t * find_unactive_session_data(
    espfsp_server_session_manager_data_t *data_set, int data_count)
{
    for (int i = 0; i < data_count; i++)
    {
        espfsp_server_session_manager_data_t *data = &data_set[i];

        if (!data->active)
        {
            return data;
        }
    }

    return NULL;
}

static espfsp_server_session_manager_data_t * find_session_data_in_dataset_by_comm_proto(
    espfsp_server_session_manager_data_t *data_set, int data_count, espfsp_comm_proto_t *comm_proto)
{
    for (int i = 0; i < data_count; i++)
    {
        espfsp_server_session_manager_data_t *data = &data_set[i];

        if (data->comm_proto == comm_proto)
        {
            return data;
        }
    }

    return NULL;
}

// static espfsp_server_session_manager_data_t * find_session_data_in_dataset_by_session_id(
//     espfsp_server_session_manager_data_t *data_set, int data_count, uint32_t session_id)
// {
//     for (int i = 0; i < data_count; i++)
//     {
//         espfsp_server_session_manager_data_t *data = &data_set[i];

//         if (data->session_id == session_id)
//         {
//             return data;
//         }
//     }

//     return NULL;
// }

static espfsp_server_session_manager_data_t* find_session_data_by_comm_proto(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto)
{
    espfsp_server_session_manager_data_t *data = NULL;

    data = find_session_data_in_dataset_by_comm_proto(
        session_manager->client_push_session_data, session_manager->client_push_session_data_count, comm_proto);
    if (data != NULL)
    {
        return data;
    }

    return find_session_data_in_dataset_by_comm_proto(
        session_manager->client_play_session_data, session_manager->client_play_session_data_count, comm_proto);
}

// static espfsp_server_session_manager_data_t* find_session_data_by_session_id(
//     espfsp_session_manager_t *session_manager, uint32_t session_id)
// {
//     espfsp_server_session_manager_data_t *data = NULL;

//     data = find_session_data_in_dataset_by_session_id(
//         session_manager->client_push_session_data, session_manager->client_push_session_data_count, session_id);
//     if (data != NULL)
//     {
//         return data;
//     }

//     return find_session_data_in_dataset_by_session_id(
//         session_manager->client_play_session_data, session_manager->client_play_session_data_count, session_id);
// }

static esp_err_t get_data_set_info(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_server_session_manager_data_t **data_set,
    int *data_count)
{
    switch (type)
    {
        case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH:
            *data_set = session_manager->client_push_session_data;
            *data_count = session_manager->client_push_session_data_count;
            return ESP_OK;

        case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY:
            *data_set = session_manager->client_play_session_data;
            *data_count = session_manager->client_play_session_data_count;
            return ESP_OK;

        default:
            ESP_LOGE(TAG, "Client type not handled");
            return ESP_FAIL;
    }
}

esp_err_t espfsp_session_manager_take(espfsp_session_manager_t *session_manager)
{
    if (xSemaphoreTake(session_manager->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot take semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_session_manager_release(espfsp_session_manager_t *session_manager)
{
    if (xSemaphoreGive(session_manager->mutex) != pdTRUE)
    {
        ESP_LOGE(TAG, "Cannot give semaphore");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espfsp_session_manager_get_comm_proto(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data_set = NULL;
    int data_count = 0;
    *comm_proto = NULL;

    ret = get_data_set_info(session_manager, type, &data_set, &data_count);
    if (ret == ESP_OK)
    {
        espfsp_server_session_manager_data_t *data = find_unactive_session_data(data_set, data_count);
        if (data != NULL)
        {
            data->active = true;
            data->session_id = UNACTIVE_SESSION_ID;
            *comm_proto = data->comm_proto;
        }
    }

    return ret;
}

esp_err_t espfsp_session_manager_return_comm_proto(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL)
    {
        data->active = false;
        data->session_id = UNACTIVE_SESSION_ID;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Return comm proto failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_activate_session(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id == UNACTIVE_SESSION_ID)
    {
        data->session_id = session_manager->config->session_id_gen(data->type);
        data->stream_started = false;
        if (snprintf(data->name, sizeof(data->name), "CLIENT_NAME-%ld", data->session_id) > sizeof(data->name))
        {
            ESP_LOGE(TAG, "Name too long");
        }
        memcpy(
            &data->frame_config,
            &session_manager->config->default_frame_config,
            sizeof(espfsp_frame_config_t));
        memcpy(
            &data->cam_config,
            &session_manager->config->default_cam_config,
            sizeof(espfsp_cam_config_t));

        // Works only with assumption of only one CLIENT_PLAY
        if (data->type == ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY)
        {
            session_manager->primary_client_play_session_data = data;
        }
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Session activation failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_deactivate_session(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        if (session_manager->primary_client_play_session_data == data)
        {
            session_manager->primary_client_play_session_data = NULL;
        }
        if (session_manager->primary_client_push_session_data == data)
        {
            session_manager->primary_client_push_session_data = NULL;
        }

        data->session_id = UNACTIVE_SESSION_ID;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Session dectivation failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_session_id(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto, uint32_t *session_id)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        *session_id = data->session_id;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get session ID failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_session_name(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto, char session_name[30])
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID && strlen(data->name) < 30)
    {
        memcpy(session_name, data->name, strlen(data->name));
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get session name failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_session_type(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_session_manager_session_type_t *session_type)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        *session_type = data->type;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get session type failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_set_stream_state(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    bool stream_started)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        data->stream_started = stream_started;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Set stream state failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_stream_state(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    bool *stream_started)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        *stream_started = data->stream_started;
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get stream state failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_frame_config(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_frame_config_t *frame_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        memcpy(frame_config, &data->frame_config, sizeof(espfsp_frame_config_t));
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get frame config failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_set_frame_config(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_frame_config_t *frame_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        memcpy(&data->frame_config, frame_config, sizeof(espfsp_frame_config_t));
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Set frame config failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_cam_config(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_cam_config_t *cam_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        memcpy(cam_config, &data->cam_config, sizeof(espfsp_cam_config_t));
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Get cam config failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_set_cam_config(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_cam_config_t *cam_config)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL && data->session_id != UNACTIVE_SESSION_ID)
    {
        memcpy(&data->cam_config, cam_config, sizeof(espfsp_cam_config_t));
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Set cam config failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_primary_session(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto)
{
    esp_err_t ret = ESP_OK;
    *comm_proto = NULL;

    switch (type)
    {
    case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH:
        if (session_manager->primary_client_push_session_data != NULL)
        {
            *comm_proto = session_manager->primary_client_push_session_data->comm_proto;
        }
        break;

    case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY:
        if (session_manager->primary_client_play_session_data != NULL)
        {
            *comm_proto = session_manager->primary_client_play_session_data->comm_proto;
        }
        break;

    default:
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Ger primary session failed. Type not handled");
    }

    return ret;
}

esp_err_t espfsp_session_manager_set_primary_session(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t *comm_proto)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data = find_session_data_by_comm_proto(session_manager, comm_proto);
    if (data != NULL)
    {
        switch (type)
        {
        case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH:
            session_manager->primary_client_push_session_data = data;
            break;

        case ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY:
            session_manager->primary_client_play_session_data = data;
            break;

        default:
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Set primary session failed. Type is not handled");
        }
    }
    else
    {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Set primary session failed");
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_active_sessions(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto_buf,
    int comm_proto_buf_len,
    int *active_sessions_count)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data_set = NULL;
    int data_count = 0;

    ret = get_data_set_info(session_manager, type, &data_set, &data_count);
    if (ret == ESP_OK)
    {
        *active_sessions_count = 0;

        for (int i = 0; i < data_count; i++)
        {
            espfsp_server_session_manager_data_t *data = &data_set[i];

            if (data->active && *active_sessions_count < comm_proto_buf_len)
            {
                comm_proto_buf[*active_sessions_count] = data->comm_proto;
                *active_sessions_count += 1;
            }
        }
    }

    return ret;
}

esp_err_t espfsp_session_manager_get_active_session(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    char session_name[30],
    espfsp_comm_proto_t **comm_proto_buf)
{
    esp_err_t ret = ESP_OK;
    espfsp_server_session_manager_data_t *data_set = NULL;
    int data_count = 0;
    *comm_proto_buf = NULL;

    char session_name_copy[30];
    memcpy(session_name_copy, session_name, sizeof(session_name_copy));
    session_name_copy[29] = '\0';
    size_t session_name_len = strlen(session_name_copy);

    ret = get_data_set_info(session_manager, type, &data_set, &data_count);
    if (ret == ESP_OK)
    {
        for (int i = 0; i < data_count; i++)
        {
            espfsp_server_session_manager_data_t *data = &data_set[i];

            if (data->active &&
                strlen(data->name) == session_name_len &&
                memcmp(data->name, session_name, session_name_len) == 0)
            {
                *comm_proto_buf = data->comm_proto;
                break;
            }
        }
    }

    return ret;
}
