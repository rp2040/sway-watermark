#ifndef RGB2YUV_H
#define RGB2YUV_H

#include <stdint.h>

// RGB转Y通道（仅提取亮度，降低色彩干扰）
void rgb2y_channel(uint8_t *rgb_frame, uint8_t *y_channel, int width, int height);

#endif