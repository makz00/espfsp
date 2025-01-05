/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"

#include "client_play/espfsp_state_def.h"

esp_err_t espfsp_client_play_comm_protos_init(espfsp_client_play_instance_t *instance);
esp_err_t espfsp_client_play_comm_protos_deinit(espfsp_client_play_instance_t *instance);
