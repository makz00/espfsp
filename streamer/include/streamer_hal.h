/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

#include <stdint.h>

typedef enum
{
    STREAMER_PIXFORMAT_RGB565,    // 2BPP/RGB565
    STREAMER_PIXFORMAT_YUV422,    // 2BPP/YUV422
    STREAMER_PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    STREAMER_PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    STREAMER_PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    STREAMER_PIXFORMAT_RGB888,    // 3BPP/RGB888
    STREAMER_PIXFORMAT_RAW,       // RAW
    STREAMER_PIXFORMAT_RGB444,    // 3BP2P/RGB444
    STREAMER_PIXFORMAT_RGB555,    // 3BP2P/RGB555
} streamer_pixformat_t;

typedef enum
{
    STREAMER_FRAMESIZE_96X96,   // 96x96
    STREAMER_FRAMESIZE_QQVGA,   // 160x120
    STREAMER_FRAMESIZE_QCIF,    // 176x144
    STREAMER_FRAMESIZE_HQVGA,   // 240x176
    STREAMER_FRAMESIZE_240X240, // 240x240
    STREAMER_FRAMESIZE_QVGA,    // 320x240
    STREAMER_FRAMESIZE_CIF,     // 400x296
    STREAMER_FRAMESIZE_HVGA,    // 480x320
    STREAMER_FRAMESIZE_VGA,     // 640x480
    STREAMER_FRAMESIZE_SVGA,    // 800x600
    STREAMER_FRAMESIZE_XGA,     // 1024x768
    STREAMER_FRAMESIZE_HD,      // 1280x720
    STREAMER_FRAMESIZE_SXGA,    // 1280x1024
    STREAMER_FRAMESIZE_UXGA,    // 1600x1200
    // 3MP Sensors
    STREAMER_FRAMESIZE_FHD,   // 1920x1080
    STREAMER_FRAMESIZE_P_HD,  //  720x1280
    STREAMER_FRAMESIZE_P_3MP, //  864x1536
    STREAMER_FRAMESIZE_QXGA,  // 2048x1536
    // 5MP Sensors
    STREAMER_FRAMESIZE_QHD,   // 2560x1440
    STREAMER_FRAMESIZE_WQXGA, // 2560x1600
    STREAMER_FRAMESIZE_P_FHD, // 1080x1920
    STREAMER_FRAMESIZE_QSXGA, // 2560x1920
    STREAMER_FRAMESIZE_INVALID
} streamer_framesize_t;

typedef enum
{
    STREAMER_GRAB_WHEN_EMPTY,
    STREAMER_GRAB_LATEST
} streamer_grab_mode_t;

typedef struct
{
    streamer_pixformat_t pixel_format;
    streamer_framesize_t frame_size;
    streamer_grab_mode_t grab_mode;
    int jpeg_quality;
    int fb_count;
} streamer_hal_config_t;

typedef struct {
    const uint16_t width;
    const uint16_t height;
} streamer_resolution_info_t;

extern const streamer_resolution_info_t streamer_resolution[];
