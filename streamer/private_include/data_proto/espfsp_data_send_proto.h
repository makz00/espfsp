/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "data_proto/espfsp_data_proto.h"

esp_err_t espfsp_data_proto_handle_send(espfsp_data_proto_t *data_proto, int sock);
