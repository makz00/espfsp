/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espfsp_server.h"
#include "espfsp_message_defs.h"
#include "espfsp_message_buffer.h"
#include "comm_proto/espfsp_comm_proto.h"
#include "data_proto/espfsp_data_proto.h"
#include "server/espfsp_session_manager.h"

#define CONFIG_ESPFSP_SERVER_MAX_INSTANCES 1
#define CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS 3
#define CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS 1

typedef struct
{
    TaskHandle_t data_send_task_handle;
    TaskHandle_t data_recv_task_handle;
    TaskHandle_t server_client_push_handle;
    TaskHandle_t server_client_play_handle;
    espfsp_server_config_t *config;
    bool used;

    espfsp_receiver_buffer_t receiver_buffer;

    espfsp_comm_proto_t client_push_comm_proto[CONFIG_ESPFSP_SERVER_CLIENT_PUSH_MAX_CONNECTIONS];
    espfsp_comm_proto_t client_play_comm_proto[CONFIG_ESPFSP_SERVER_CLIENT_PLAY_MAX_CONNECTIONS];

    espfsp_data_proto_t client_push_data_proto;
    espfsp_data_proto_t client_play_data_proto;

    espfsp_session_manager_t session_manager;
} espfsp_server_instance_t;

typedef struct
{
    espfsp_server_instance_t instances[CONFIG_ESPFSP_SERVER_MAX_INSTANCES];
} espfsp_server_state_t;
