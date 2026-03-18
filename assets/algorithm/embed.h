#ifndef EMBED_H
#define EMBED_H

#include <stdint.h>
#include "../config/device_info.h"

// 读取watermark.conf配置文件
int read_watermark_config(const char *config_path, WatermarkConfig *config);

// 生成实时水印模板（设备ID+当前时间戳）
int generate_realtime_template(uint8_t *template_data, int template_w, int template_h, 
                               WatermarkConfig *config, uint32_t timestamp);

// 水印嵌入主函数（集成到Sway帧渲染）
int watermark_embed(uint8_t *rgb_frame, int width, int height, const char *config_path);

#endif