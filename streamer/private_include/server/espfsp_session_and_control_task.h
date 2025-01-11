/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "espfsp_config.h"
#include "server/espfsp_session_manager.h"

typedef struct {
    espfsp_session_manager_t *session_manager;
    espfsp_session_manager_session_type_t session_type;
    int server_port;
    espfsp_task_info_t connection_task_info;
} espfsp_server_session_and_control_task_data_t;

// Pointer passed to this task has to point to structure espfsp_server_session_and_control_task_data_t
// After invocation, this task takes responsibility for passed memory, so also it deallocates it
void espfsp_server_session_and_control_task(void *pvParameters);
