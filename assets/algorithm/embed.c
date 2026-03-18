#include "embed.h"
#include "rgb2yuv.h"
#include "sobel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

// 去除字符串首尾空格（配置解析辅助）
static void trim(char *str) {
    char *start = str;
    char *end = str + strlen(str) - 1;
    while (isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

// 读取配置文件（完整解析watermark.conf）
int read_watermark_config(const char *config_path, WatermarkConfig *config) {
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "Open config failed: %s\n", config_path);
        return -1;
    }

    // 默认值初始化
    config->edge_threshold = 20;
    config->alpha_edge = 0.05f;
    config->alpha_smooth = 0.02f;
    config->template_width = 400;
    config->template_height = 200;
    config->template_redundancy = 3;
    config->pixel_spacing = 8;
    config->device_id = DEVICE_ID;
    strcpy(config->timestamp_mode, "realtime");
    config->fixed_timestamp = 1718000000;

    char line[256], section[32] = {0};
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (strlen(line) == 0 || line[0] == '#') continue;

        // 解析章节 [xxx]
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            strncpy(section, line+1, strlen(line)-2);
            section[strlen(line)-2] = '\0';
            trim(section);
            continue;
        }

        // 解析键值对 key = value
        char *equal = strchr(line, '=');
        if (!equal) continue;
        char key[64], value[64];
        strncpy(key, line, equal - line);
        strncpy(value, equal + 1, strlen(line) - (equal - line) - 1);
        trim(key);
        trim(value);

        // 赋值到配置结构体
        if (strcmp(section, "edge") == 0) {
            if (strcmp(key, "threshold") == 0) config->edge_threshold = atoi(value);
            else if (strcmp(key, "alpha_edge") == 0) config->alpha_edge = atof(value);
            else if (strcmp(key, "alpha_smooth") == 0) config->alpha_smooth = atof(value);
        } else if (strcmp(section, "template") == 0) {
            if (strcmp(key, "width") == 0) config->template_width = atoi(value);
            else if (strcmp(key, "height") == 0) config->template_height = atoi(value);
            else if (strcmp(key, "redundancy") == 0) config->template_redundancy = atoi(value);
            else if (strcmp(key, "pixel_spacing") == 0) config->pixel_spacing = atoi(value);
        } else if (strcmp(section, "device") == 0) {
            if (strcmp(key, "id") == 0) {
                if (strstr(value, "0x") == value) sscanf(value, "0x%X", &config->device_id);
                else config->device_id = atoi(value);
            } else if (strcmp(key, "timestamp_mode") == 0) strncpy(config->timestamp_mode, value, 15);
            else if (strcmp(key, "fixed_timestamp") == 0) config->fixed_timestamp = atoi(value);
        }
    }

    fclose(fp);
    return 0;
}

// 生成实时模板（设备ID+当前时间戳→64bit→伪随机点阵）
int generate_realtime_template(uint8_t *template_data, int template_w, int template_h, 
                               WatermarkConfig *config, uint32_t timestamp) {
    if (!template_data || !config) return -1;

    memset(template_data, 0, template_w * template_h);
    int template_size = template_w * template_h;

    // 拼接64bit串：前32bit=设备ID，后32bit=时间戳
    uint64_t watermark_data = ((uint64_t)config->device_id << 32) | (uint64_t)timestamp;

    // 伪随机生成64个不重复位置（设备ID为种子，保证分布固定）
    srand(config->device_id);
    int positions[64][2];
    for (int i = 0; i < 64; i++) {
        int pos = rand() % template_size;
        int duplicate = 0;
        for (int j = 0; j < i; j++) {
            if (positions[j][0] * template_w + positions[j][1] == pos) {
                duplicate = 1;
                i--;
                break;
            }
        }
        if (duplicate) continue;
        positions[i][0] = pos / template_w; // Y坐标
        positions[i][1] = pos % template_w; // X坐标
    }

    // 赋值：1→255，0→0
    for (int i = 0; i < 64; i++) {
        int y = positions[i][0], x = positions[i][1];
        uint64_t bit = (watermark_data >> (63 - i)) & 1;
        template_data[y * template_w + x] = bit ? 255 : 0;
    }

    return 0;
}

// 加载/生成模板（realtime=动态生成，fixed=读取文件）
static int load_or_generate_template(uint8_t *template_data, WatermarkConfig *config, uint32_t timestamp) {
    if (strcmp(config->timestamp_mode, "realtime") == 0) {
        return generate_realtime_template(template_data, config->template_width, 
                                         config->template_height, config, timestamp);
    } else {
        FILE *fp = fopen(WATERMARK_TEMPLATE_PATH, "rb");
        if (!fp) return -1;
        fread(template_data, 1, config->template_width * config->template_height, fp);
        fclose(fp);
        return 0;
    }
}

// 核心嵌入函数（集成到Sway每帧渲染）
int watermark_embed(uint8_t *rgb_frame, int width, int height, const char *config_path) {
    if (!rgb_frame || !config_path) return -1;
    // 0. 调试：打印帧信息，确认拿到了合成帧
    printf("合成帧：宽度=%d,高度=%d,帧数据指针=%p\n", width, height, rgb_frame);
    // 1. 读取配置
    WatermarkConfig config;
    if (read_watermark_config(config_path, &config) < 0) return -1;

    // 2. 获取实时时间戳（UTC秒级）
    uint32_t current_ts = (strcmp(config.timestamp_mode, "realtime") == 0) ? 
                          (uint32_t)time(NULL) : config.fixed_timestamp;

    // 3. RGB转Y通道（仅处理亮度）
    uint8_t *y_channel = (uint8_t *)malloc(width * height);
    uint8_t *edge_mask = (uint8_t *)malloc(width * height);
    if (!y_channel || !edge_mask) {
        free(y_channel); free(edge_mask);
        return -1;
    }
    rgb2y_channel(rgb_frame, y_channel, width, height);
    sobel_edge_detect(y_channel, edge_mask, width, height, config.edge_threshold);

    // 4. 生成模板（内存中）
    int template_size = config.template_width * config.template_height;
    uint8_t *template_data = (uint8_t *)malloc(template_size);
    if (!template_data) {
        free(y_channel); free(edge_mask); free(template_data);
        return -1;
    }
    if (load_or_generate_template(template_data, &config, current_ts) < 0) {
        free(y_channel); free(edge_mask); free(template_data);
        return -1;
    }

    // 5. 冗余嵌入（3次）
    srand(config.device_id + current_ts); // 随机偏移种子
    for (int r = 0; r < config.template_redundancy; r++) {
        // 随机偏移（避免重叠）
        int offset_x = (rand() % (width - config.template_width)) / config.pixel_spacing * config.pixel_spacing;
        int offset_y = (rand() % (height - config.template_height)) / config.pixel_spacing * config.pixel_spacing;

        // 逐点嵌入
        for (int ty = 0; ty < config.template_height; ty += config.pixel_spacing) {
            for (int tx = 0; tx < config.template_width; tx += config.pixel_spacing) {
                int frame_x = offset_x + tx;
                int frame_y = offset_y + ty;
                if (frame_x >= width || frame_y >= height) continue;

                // 自适应强度（边缘区/纯色区）
                uint8_t mask = edge_mask[frame_y * width + frame_x];
                float alpha = mask ? config.alpha_edge : config.alpha_smooth;

                // 模板值筛选（仅处理1的位置）
                uint8_t t_val = template_data[ty * config.template_width + tx];
                if (t_val == 0) continue;

                // 调整亮度并还原RGB
                int y_idx = frame_y * width + frame_x;
                uint8_t old_y = y_channel[y_idx];
                uint8_t new_y = (uint8_t)(old_y + alpha * 255);
                new_y = new_y > 255 ? 255 : new_y;

                // YUV转RGB（保留原始UV，仅调整Y）
                int rgb_idx = y_idx * 3;
                float r = rgb_frame[rgb_idx] / 255.0f;
                float g = rgb_frame[rgb_idx+1] / 255.0f;
                float b = rgb_frame[rgb_idx+2] / 255.0f;

                float u = -0.14713f * r - 0.28886f * g + 0.436f * b;
                float v = 0.615f * r - 0.51499f * g - 0.10001f * b;
                float new_r = (new_y / 255.0f) + 1.13983f * v;
                float new_g = (new_y / 255.0f) - 0.39465f * u - 0.58060f * v;
                float new_b = (new_y / 255.0f) + 2.03211f * u;

                // 赋值（防溢出）
                rgb_frame[rgb_idx] = (uint8_t)(fminf(fmaxf(new_r * 255, 0), 255));
                rgb_frame[rgb_idx+1] = (uint8_t)(fminf(fmaxf(new_g * 255, 0), 255));
                rgb_frame[rgb_idx+2] = (uint8_t)(fminf(fmaxf(new_b * 255, 0), 255));
            }
        }
    }

    // 释放内存
    free(y_channel);
    free(edge_mask);
    free(template_data);
    return 0;
}