set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(srcs
    espfsp_client_play.c
    espfsp_client_push.c
    espfsp_server.c
    espfsp_sock_op.c

)

set(priv_include_dirs
    streamer/private_include
)

set(include_dirs
    streamer/include
    streamer/include/camera
    streamer/include/central
    streamer/include/client
)

set(priv_requires
    freertos spi_flash esp_timer esp_netif
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    PRIV_REQUIRES ${priv_requires}
)
