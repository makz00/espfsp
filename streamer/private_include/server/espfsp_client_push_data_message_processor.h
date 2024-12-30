/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "espfsp_message_defs.h"

void espfsp_server_client_push_process_message(const espfsp_message_t *message, espfsp_server_instance_t *instance);
