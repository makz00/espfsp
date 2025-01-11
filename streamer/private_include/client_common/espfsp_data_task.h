/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_netif.h"

#include "data_proto/espfsp_data_proto.h"

typedef struct {
    espfsp_data_proto_t *data_proto;
    int local_port;
    int remote_port;
    struct esp_ip4_addr remote_addr;
} espfsp_client_data_task_data_t;

// Pointer passed to this task has to point to structure espfsp_client_data_task_data_t
// After invocation, this task takes responsibility for passed memory, so also it deallocates it
void espfsp_client_data_task(void *pvParameters);
