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
    streamer/central/streamer_central_camera_receiver.c
    streamer/central/streamer_central_camera_receiver_message_processor.c
    streamer/central/streamer_central_remote_accessor_controler.c
    streamer/central/streamer_central_remote_accessor_sender.c
    # Client
    streamer/client/streamer_client.c
    streamer/client/streamer_client_central_controler.c
    streamer/client/streamer_client_central_receiver_message_processor.c
    streamer/client/streamer_client_central_receiver.c
)

set(priv_include_dirs
    # Common
    streamer/private_include
    # Camera
    streamer/private_include/camera
    # Central
    streamer/private_include/central
    # Client
    streamer/private_include/client
)

set(include_dirs
    # Common
    streamer/include
    # Camera
    streamer/include/camera
    # Central
    streamer/include/central
    # Client
    streamer/include/client
)

set(priv_requires
    freertos spi_flash esp_timer esp_netif
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    # REQUIRES driver  # due to include of driver/gpio.h in esp_camera.h
    PRIV_REQUIRES ${priv_requires}
)
