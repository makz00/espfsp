/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#pragma once

typedef enum
{
    ESPFSP_PIXFORMAT_RGB565,    // 2BPP/RGB565
    ESPFSP_PIXFORMAT_YUV422,    // 2BPP/YUV422
    ESPFSP_PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    ESPFSP_PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    ESPFSP_PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    ESPFSP_PIXFORMAT_RGB888,    // 3BPP/RGB888
    ESPFSP_PIXFORMAT_RAW,       // RAW
    ESPFSP_PIXFORMAT_RGB444,    // 3BP2P/RGB444
    ESPFSP_PIXFORMAT_RGB555,    // 3BP2P/RGB555
} espfsp_pixformat_t;

typedef enum
{
    ESPFSP_FRAMESIZE_96X96,   // 96x96
    ESPFSP_FRAMESIZE_QQVGA,   // 160x120
    ESPFSP_FRAMESIZE_QCIF,    // 176x144
    ESPFSP_FRAMESIZE_HQVGA,   // 240x176
    ESPFSP_FRAMESIZE_240X240, // 240x240
    ESPFSP_FRAMESIZE_QVGA,    // 320x240
    ESPFSP_FRAMESIZE_CIF,     // 400x296
    ESPFSP_FRAMESIZE_HVGA,    // 480x320
    ESPFSP_FRAMESIZE_VGA,     // 640x480
    ESPFSP_FRAMESIZE_SVGA,    // 800x600
    ESPFSP_FRAMESIZE_XGA,     // 1024x768
    ESPFSP_FRAMESIZE_HD,      // 1280x720
    ESPFSP_FRAMESIZE_SXGA,    // 1280x1024
    ESPFSP_FRAMESIZE_UXGA,    // 1600x1200
    // 3MP Sensors
    ESPFSP_FRAMESIZE_FHD,   // 1920x1080
    ESPFSP_FRAMESIZE_P_HD,  //  720x1280
    ESPFSP_FRAMESIZE_P_3MP, //  864x1536
    ESPFSP_FRAMESIZE_QXGA,  // 2048x1536
    // 5MP Sensors
    ESPFSP_FRAMESIZE_QHD,   // 2560x1440
    ESPFSP_FRAMESIZE_WQXGA, // 2560x1600
    ESPFSP_FRAMESIZE_P_FHD, // 1080x1920
    ESPFSP_FRAMESIZE_QSXGA, // 2560x1920
    ESPFSP_FRAMESIZE_INVALID
} espfsp_framesize_t;

typedef enum
{
    ESPFSP_GRAB_WHEN_EMPTY,
    ESPFSP_GRAB_LATEST
} espfsp_grab_mode_t;

typedef struct
{
    espfsp_grab_mode_t cam_grab_mode;
    espfsp_pixformat_t cam_pixel_format;
    espfsp_framesize_t cam_frame_size;
    int cam_jpeg_quality;
    int cam_fb_count;
} espfsp_cam_config_t;
