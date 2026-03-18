#include "rgb2yuv.h"
#include <string.h>

void rgb2y_channel(uint8_t *rgb_frame, uint8_t *y_channel, int width, int height) {
    if (!rgb_frame || !y_channel) return;

    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i++) {
        // RGB分量（8bit）
        uint8_t r = rgb_frame[i * 3];
        uint8_t g = rgb_frame[i * 3 + 1];
        uint8_t b = rgb_frame[i * 3 + 2];
        
        // BT.601标准：Y = 0.299R + 0.587G + 0.114B
        uint8_t y = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
        y_channel[i] = y;
    }
}