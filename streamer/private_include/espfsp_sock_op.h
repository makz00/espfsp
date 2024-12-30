/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include "esp_err.h"
#include "esp_netif.h"

#include "espfsp_config.h"

void espfsp_set_addr(struct sockaddr_in *addr, const struct esp_ip4_addr *esp_addr, int port);
void espfsp_set_local_addr(struct sockaddr_in *addr, int port);

esp_err_t espfsp_udp_send_whole_fb(int sock, espfsp_fb_t *fb);
esp_err_t espfsp_udp_send_whole_fb_to(int sock, espfsp_fb_t *fb, struct sockaddr *dest_addr);
esp_err_t espfsp_udp_receive(int sock, char *rx_buffer, int rx_buffer_len);

esp_err_t espfsp_tcp_accept(int *listen_sock, int *sock, struct sockaddr_in *source_addr, socklen_t *addr_len);

esp_err_t espfsp_create_tcp_server(int *sock, int port);
esp_err_t espfsp_create_tcp_client(int *sock, int client_port, struct sockaddr_in *server_addr);

esp_err_t espfsp_create_udp_server(int *sock, int port);
esp_err_t espfsp_create_udp_client(int *sock, int port, struct sockaddr_in *server_addr);

esp_err_t espfsp_remove_host(int sock);
