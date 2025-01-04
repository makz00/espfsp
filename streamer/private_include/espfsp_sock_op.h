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

esp_err_t espfsp_send_whole_fb(int sock, espfsp_fb_t *fb);
esp_err_t espfsp_send_whole_fb_to(int sock, espfsp_fb_t *fb, struct sockaddr_in *dest_addr);

esp_err_t espfsp_send(int sock, char *rx_buffer, int rx_buffer_len);
esp_err_t espfsp_send_to(int sock, char *rx_buffer, int rx_buffer_len, struct sockaddr_in *source_addr);

esp_err_t espfsp_receive_bytes(int sock, char *rx_buffer, int rx_buffer_len);
esp_err_t espfsp_receive_bytes_from(
    int sock, char *rx_buffer, int rx_buffer_len, struct sockaddr_in *source_addr, socklen_t *addr_len);

esp_err_t espfsp_receive_block(int sock, char *rx_buffer, int rx_buffer_len, int *received, struct timeval *timeout);
esp_err_t espfsp_receive_from_block(
    int sock,
    char *rx_buffer,
    int rx_buffer_len,
    int *received,
    struct timeval *timeout,
    struct sockaddr_in *source_addr,
    socklen_t *addr_len);

esp_err_t espfsp_receive_no_block(int sock, char *rx_buffer, int rx_buffer_len, int *received);
esp_err_t espfsp_receive_from_no_block(
    int sock, char *rx_buffer, int rx_buffer_len, int *received, struct sockaddr_in *source_addr, socklen_t *addr_len);

esp_err_t espfsp_connect(int sock, struct sockaddr_in *addr);
esp_err_t espfsp_tcp_accept(int listen_sock, int *sock, struct sockaddr_in *source_addr, socklen_t *addr_len);

esp_err_t espfsp_create_tcp_server(int *sock, int port);
esp_err_t espfsp_create_tcp_client(int *sock, int client_port, struct sockaddr_in *server_addr);

esp_err_t espfsp_create_udp_server(int *sock, int port);
esp_err_t espfsp_create_udp_client(int *sock, int port, struct sockaddr_in *server_addr);

esp_err_t espfsp_remove_host(int sock);
