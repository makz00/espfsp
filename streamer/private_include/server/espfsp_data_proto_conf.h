/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"

#include "server/espfsp_state_def.h"

esp_err_t espfsp_server_data_protos_init(espfsp_server_instance_t *instance);
esp_err_t espfsp_server_data_protos_deinit(espfsp_server_instance_t *instance);
