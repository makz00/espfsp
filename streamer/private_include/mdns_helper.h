/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_netif.h"

typedef int esp_err_t;

esp_err_t wait_for_server(const char *hostname, struct esp_ip4_addr *addr, char * addr_str, int addr_str_len);
