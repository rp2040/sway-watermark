#ifndef SOBEL_H
#define SOBEL_H

#include <stdint.h>

// Sobel边缘检测（生成边缘掩码：1=边缘区，0=纯色区）
void sobel_edge_detect(uint8_t *y_channel, uint8_t *edge_mask, int width, int height, int threshold);

#endif