#include "sobel.h"
#include <stdlib.h>
#include <math.h>

void sobel_edge_detect(uint8_t *y_channel, uint8_t *edge_mask, int width, int height, int threshold) {
    if (!y_channel || !edge_mask) return;

    // Sobel算子（X/Y方向）
    int sobel_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int sobel_y[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    // 初始化边缘掩码为0
    memset(edge_mask, 0, width * height);

    // 逐像素计算梯度（跳过边界）
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int gx = 0, gy = 0;
            // 卷积计算
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = y_channel[(y + ky) * width + (x + kx)];
                    gx += pixel * sobel_x[ky + 1][kx + 1];
                    gy += pixel * sobel_y[ky + 1][kx + 1];
                }
            }
            // 梯度幅值
            int grad = (int)sqrt(gx * gx + gy * gy);
            // 阈值筛选：超过阈值=边缘区（1）
            edge_mask[y * width + x] = (grad > threshold) ? 1 : 0;
        }
    }
}