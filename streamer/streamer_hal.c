/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "streamer_hal.h"

const streamer_resolution_info_t streamer_resolution[STREAMER_FRAMESIZE_INVALID] = {
    {   96,   96 }, /* 96x96 */
    {  160,  120 }, /* QQVGA */
    {  176,  144 }, /* QCIF  */
    {  240,  176 }, /* HQVGA */
    {  240,  240 }, /* 240x240 */
    {  320,  240 }, /* QVGA  */
    {  400,  296 }, /* CIF   */
    {  480,  320 }, /* HVGA  */
    {  640,  480 }, /* VGA   */
    {  800,  600 }, /* SVGA  */
    { 1024,  768 }, /* XGA   */
    { 1280,  720 }, /* HD    */
    { 1280, 1024 }, /* SXGA  */
    { 1600, 1200 }, /* UXGA  */
    // 3MP Sensors
    { 1920, 1080 }, /* FHD   */
    {  720, 1280 }, /* Portrait HD   */
    {  864, 1536 }, /* Portrait 3MP   */
    { 2048, 1536 }, /* QXGA  */
    // 5MP Sensors
    { 2560, 1440 }, /* QHD    */
    { 2560, 1600 }, /* WQXGA  */
    { 1088, 1920 }, /* Portrait FHD   */
    { 2560, 1920 }, /* QSXGA  */
};
