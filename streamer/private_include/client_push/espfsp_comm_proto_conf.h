/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"

#include "client_push/espfsp_state_def.h"

esp_err_t espfsp_client_push_comm_protos_init(espfsp_client_push_instance_t *instance);
esp_err_t espfsp_client_push_comm_protos_deinit(espfsp_client_push_instance_t *instance);
