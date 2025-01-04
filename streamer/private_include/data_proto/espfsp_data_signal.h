/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"

#include "data_proto/espfsp_data_proto.h"

// Functions intended for NAT hole punching
// CLIEN_PLAY wants data from server, but in most cases client will be behind NAT. In order to resolve this problem:
//
// - client has to send signal to server
// - server has to receive signal and keep address of sender (because of NAT it will be different than local address)
// - server has to send data for received address
// - client and server have to send signals from time to time, as downtime moments could occure

esp_err_t espfsp_data_proto_handle_incoming_signal(espfsp_data_proto_t *data_proto, int sock);
esp_err_t espfsp_data_proto_handle_outcoming_signal(espfsp_data_proto_t *data_proto, int sock);
