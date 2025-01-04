/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "data_proto/espfsp_data_proto.h"
#include "comm_proto/espfsp_comm_proto.h"

typedef struct {
    espfsp_data_proto_t *data_proto;
    espfsp_comm_proto_t *comm_proto;
    int port;
} espfsp_data_task_data_t;

// Pointer passed to this task has to point to structure espfsp_data_task_data_t
// After invocation, this task takes responsibility for passed memory, so also it deallocates it
void espfsp_server_data_task(void *pvParameters);
