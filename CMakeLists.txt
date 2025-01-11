set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(srcs
    streamer/espfsp_client_play.c
    streamer/espfsp_client_push.c
    streamer/espfsp_server.c
    streamer/espfsp_sock_op.c
    streamer/espfsp_message_buffer.c
    streamer/espfsp_stream_manager.c

    streamer/comm_proto/espfsp_comm_proto.c

    streamer/data_proto/espfsp_data_proto.c
    streamer/data_proto/espfsp_data_recv_proto.c
    streamer/data_proto/espfsp_data_send_proto.c
    streamer/data_proto/espfsp_data_signal.c

    streamer/client_common/espfsp_data_task.c
    streamer/client_common/espfsp_session_and_control_task.c

    streamer/client_play/espfsp_comm_proto_conf.c
    streamer/client_play/espfsp_comm_proto_handlers.c
    streamer/client_play/espfsp_data_proto_conf.c

    streamer/client_push/espfsp_comm_proto_conf.c
    streamer/client_push/espfsp_comm_proto_handlers.c
    streamer/client_push/espfsp_data_proto_conf.c

    streamer/server/espfsp_comm_proto_conf.c
    streamer/server/espfsp_comm_proto_handlers.c
    streamer/server/espfsp_data_proto_conf.c
    streamer/server/espfsp_data_task.c
    streamer/server/espfsp_session_and_control_task.c
    streamer/server/espfsp_session_manager.c
)

set(priv_include_dirs
    streamer/private_include
)

set(include_dirs
    streamer/include
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
