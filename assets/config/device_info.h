#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <stdint.h>

// 设备唯一标识（烧录到设备的固定值，每台设备不同）
#define DEVICE_ID 0x1A2B3C4D  

// 路径配置（替换为你的实际绝对路径）
#define WATERMARK_CONFIG_PATH "/home/sdq/sway/sway-1.2/assets/config/watermark.conf"
#define WATERMARK_TEMPLATE_PATH "/home/sdq/sway/sway-1.2/assets/template/watermark_template.bin"

// 水印配置结构体（映射watermark.conf）
typedef struct {
    // 边缘检测参数
    int edge_threshold;
    float alpha_edge;
    float alpha_smooth;
    // 模板参数
    int template_width;
    int template_height;
    int template_redundancy;
    int pixel_spacing;
    // 设备与时间戳参数
    uint32_t device_id;
    char timestamp_mode[16]; // "realtime" / "fixed"
    uint32_t fixed_timestamp;
} WatermarkConfig;

// 读取配置文件函数声明
int read_watermark_config(const char *config_path, WatermarkConfig *config);

#endif