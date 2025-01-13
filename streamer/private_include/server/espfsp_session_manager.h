/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "comm_proto/espfsp_comm_proto.h"

#define ESPFSP_SERVER_SESSION_NAME_MAX_LEN 30

// Current simplified assumptions about sessions:
// - only one session per connection is allowed;
// - client type passed in session init message is (will be) only for
//   validation purpose, but generally client type is recognized based on port
//   on which it talk;
// - there can be maximum client_push_max_connections of CLIENT_PUSH sessions
//   and client_play_max_connections of CLIENT_PLAY sessions, but for each
//   client type only one session (so one client) is primary; primary means
//   that only one CLIENT_PLAY can receive DATA and only one CLIENT_PUSH can
//   send DATA;

typedef enum
{
    ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PUSH,
    ESPFSP_SESSION_MANAGER_SESSION_TYPE_CLIENT_PLAY,
} espfsp_session_manager_session_type_t;

typedef struct
{
    espfsp_comm_proto_t *comm_proto;
    espfsp_session_manager_session_type_t type;
    uint32_t session_id;
    bool active;
    char name[ESPFSP_SERVER_SESSION_NAME_MAX_LEN];
    bool stream_started;
} espfsp_server_session_manager_data_t;

typedef uint32_t (*__espfsp_session_manager_session_id_generator)(espfsp_session_manager_session_type_t type);

// Server Session Manager interface assumes that Communication Protocols are
// initiated and hold by some owner (server instance) and will be hold as long
// as Server Session Manager exist.
typedef struct
{
    espfsp_comm_proto_t *client_push_comm_protos;
    espfsp_comm_proto_t *client_play_comm_protos;
    int client_push_comm_protos_count;
    int client_play_comm_protos_count;
    __espfsp_session_manager_session_id_generator session_id_gen;
} espfsp_server_session_manager_config_t;

typedef struct
{
    espfsp_server_session_manager_config_t *config;
    espfsp_server_session_manager_data_t *client_push_session_data;
    espfsp_server_session_manager_data_t *client_play_session_data;
    int client_push_session_data_count;
    int client_play_session_data_count;

    SemaphoreHandle_t mutex;
    espfsp_server_session_manager_data_t *primary_client_push_session_data;
    espfsp_server_session_manager_data_t *primary_client_play_session_data;
} espfsp_session_manager_t;

// Initialization/Deinitialization of Session Manager
esp_err_t espfsp_session_manager_init(
    espfsp_session_manager_t *session_manager, espfsp_server_session_manager_config_t *config);
esp_err_t espfsp_session_manager_deinit(espfsp_session_manager_t *session_manager);

// Synchronization mechanism for safe access - tasks can use same Session Manager.
// Do not take Session Manager for long time - only few noblocking functions calls.
esp_err_t espfsp_session_manager_take(espfsp_session_manager_t *session_manager);
esp_err_t espfsp_session_manager_release(espfsp_session_manager_t *session_manager);

// Get and return Session Manager resources for connection/session
esp_err_t espfsp_session_manager_get_comm_proto(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto);
esp_err_t espfsp_session_manager_return_comm_proto(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto);

// Session management functions for Communication Protocol
esp_err_t espfsp_session_manager_activate_session(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto);
esp_err_t espfsp_session_manager_deactivate_session(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto);
esp_err_t espfsp_session_manager_get_session_id(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto, uint32_t *session_id);
esp_err_t espfsp_session_manager_get_session_name(
    espfsp_session_manager_t *session_manager, espfsp_comm_proto_t *comm_proto, char **session_name);
esp_err_t espfsp_session_manager_get_session_type(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    espfsp_session_manager_session_type_t *session_type);
esp_err_t espfsp_session_manager_set_stream_state(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    bool stream_started);
esp_err_t espfsp_session_manager_get_stream_state(
    espfsp_session_manager_t *session_manager,
    espfsp_comm_proto_t *comm_proto,
    bool *stream_started);

// General management of Session Manager
esp_err_t espfsp_session_manager_get_primary_session(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto);
esp_err_t espfsp_session_manager_set_primary_session(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t *comm_proto);
esp_err_t espfsp_session_manager_get_active_sessions(
    espfsp_session_manager_t *session_manager,
    espfsp_session_manager_session_type_t type,
    espfsp_comm_proto_t **comm_proto_buf,
    int comm_proto_buf_len,
    int *active_sessions_count);
