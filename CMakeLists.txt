set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(srcs
    # Common
    streamer/mdns_helper.c
    streamer/streamer_hal.c
    # Camera
    streamer/camera/streamer_camera.c
    streamer/camera/streamer_camera_controler.c
    streamer/camera/streamer_camera_sender.c
    # Central
    streamer/central/streamer_central.c
    streamer/central/streamer_central_camera_controler.c
    streamer/central/streamer_central_receiver.c
    streamer/central/streamer_central_receiver_message_processor.c
    streamer/central/streamer_central_client_controler.c
    streamer/central/streamer_central_sender.c
    # Client
    streamer/client/streamer_client.c
    streamer/client/streamer_client_controler.c
    streamer/client/streamer_client_receiver_message_processor.c
    streamer/client/streamer_client_receiver.c
    # Protocol
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
