#ifndef DETECT_H
#define DETECT_H

#include <stdint.h>

// 水印检测主函数（无设备ID先验）
int watermark_detect(uint8_t *rgb_img, int img_w, int img_h,
                     uint32_t *device_id, uint32_t *timestamp, char *time_str);

#endif