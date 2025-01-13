/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_netif.h"

#include "comm_proto/espfsp_comm_proto.h"

typedef struct {
    espfsp_comm_proto_t *comm_proto;
    espfsp_comm_proto_req_client_type_t client_type;
    int local_port;
    int remote_port;
    struct esp_ip4_addr remote_addr;
} espfsp_client_session_and_control_task_data_t;

// Pointer passed to this task has to point to structure espfsp_client_session_and_control_task_data_t
// After invocation, this task takes responsibility for passed memory, so also it deallocates it
void espfsp_client_session_and_control_task(void *pvParameters);
