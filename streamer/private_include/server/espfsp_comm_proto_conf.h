/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "server/espfsp_state_def.h"

esp_err_t espfsp_server_comm_protos_init(espfsp_server_instance_t *instance);
esp_err_t espfsp_server_comm_protos_deinit(espfsp_server_instance_t *instance);
