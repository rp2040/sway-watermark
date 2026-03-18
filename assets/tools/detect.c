#include "detect.h"
#include "bch.h"
#include "../algorithm/rgb2yuv.h"
#include "../config/device_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// 全局配置（从配置文件读取）
static int TEMPLATE_W = 400;
static int TEMPLATE_H = 200;
static int PIXEL_SPACING = 8;
static int BIT_COUNT = 64;
static int REDUNDANCY = 3;
static float MATCH_THRESHOLD = 0.7;

// 滑窗特征检测（判断是否为水印区）
static float detect_dot_matrix_feature(uint8_t *y_block, int w, int h, int spacing) {
    int total_pixels = w * h;
    int bright = 0, dark = 0, mean = 0;
    for (int i = 0; i < total_pixels; i++) mean += y_block[i];
    mean /= total_pixels;

    for (int y = 0; y < h; y += spacing) {
        for (int x = 0; x < w; x += spacing) {
            int idx = y * w + x;
            if (y_block[idx] > mean + 10) bright++;
            if (y_block[idx] < mean - 10) dark++;
        }
    }

    return (float)(bright + dark) / ((w/spacing) * (h/spacing));
}

// 全局滑窗定位水印区
static int find_watermark_regions(uint8_t *y_channel, int img_w, int img_h,
                                 int reg_w, int reg_h, int n,
                                 int *match_x, int *match_y, float *match_scores) {
    if (n <= 0 || reg_w >= img_w || reg_h >= img_h) return -1;

    for (int i = 0; i < n; i++) {
        match_x[i] = 0; match_y[i] = 0; match_scores[i] = 0.0f;
    }

    int step = 16;
    int block_size = reg_w * reg_h;
    uint8_t *y_block = (uint8_t *)malloc(block_size);
    if (!y_block) return -1;

    for (int y = 0; y <= img_h - reg_h; y += step) {
        for (int x = 0; x <= img_w - reg_w; x += step) {
            // 提取滑窗数据
            for (int ty = 0; ty < reg_h; ty++) {
                memcpy(y_block + ty * reg_w,
                       y_channel + (y + ty) * img_w + x,
                       reg_w);
            }

            // 特征匹配
            float score = detect_dot_matrix_feature(y_block, reg_w, reg_h, PIXEL_SPACING);
            if (score < MATCH_THRESHOLD) continue;

            // 保留TOP-N
            for (int i = 0; i < n; i++) {
                if (score > match_scores[i]) {
                    for (int j = n-1; j > i; j--) {
                        match_x[j] = match_x[j-1];
                        match_y[j] = match_y[j-1];
                        match_scores[j] = match_scores[j-1];
                    }
                    match_x[i] = x;
                    match_y[i] = y;
                    match_scores[i] = score;
                    break;
                }
            }
        }
    }

    free(y_block);
    int valid = 0;
    for (int i = 0; i < n; i++) if (match_scores[i] > 0) valid++;
    return valid;
}

// 点阵采样（还原64bit串）
static int sample_dot_matrix(uint8_t *y_channel, int img_w, int img_h,
                            int reg_x, int reg_y, int reg_w, int reg_h,
                            uint64_t *binary_data) {
    if (!binary_data) return -1;
    *binary_data = 0;

    int sample_x[64], sample_y[64], idx = 0;
    int mean = 0, block_size = reg_w * reg_h;
    uint8_t *y_block = (uint8_t *)malloc(block_size);
    if (!y_block) return -1;

    // 提取区域数据并计算均值
    for (int ty = 0; ty < reg_h; ty++) {
        memcpy(y_block + ty * reg_w,
               y_channel + (reg_y + ty) * img_w + reg_x,
               reg_w);
        for (int tx = 0; tx < reg_w; tx++) mean += y_block[ty * reg_w + tx];
    }
    mean /= block_size;

    // 采样亮度突变点
    for (int y = 0; y < reg_h && idx < 64; y += PIXEL_SPACING) {
        for (int x = 0; x < reg_w && idx < 64; x += PIXEL_SPACING) {
            int pos = y * reg_w + x;
            if (abs(y_block[pos] - mean) > 8) {
                sample_x[idx] = x;
                sample_y[idx] = y;
                idx++;
            }
        }
    }

    // 补充随机点（采样不足）
    for (int i = idx; i < 64; i++) {
        sample_x[i] = rand() % (reg_w - PIXEL_SPACING);
        sample_y[i] = rand() % (reg_h - PIXEL_SPACING);
    }

    // 二值化
    for (int i = 0; i < 64; i++) {
        int x = sample_x[i], y = sample_y[i];
        int pos = (reg_y + y) * img_w + (reg_x + x);
        uint64_t bit = (y_channel[pos] > mean) ? 1 : 0;
        *binary_data |= (bit << (63 - i));
    }

    free(y_block);
    return 0;
}

// 冗余投票+纠错
static int vote_and_correct(uint64_t *binary_arr, int arr_len, uint64_t *valid_data) {
    if (arr_len <= 0 || !binary_arr || !valid_data) return -1;
    *valid_data = 0;

    // 逐位投票
    for (int i = 0; i < 64; i++) {
        int cnt = 0;
        for (int j = 0; j < arr_len; j++) {
            if ((binary_arr[j] >> (63 - i)) & 1) cnt++;
        }
        if (cnt > arr_len / 2) *valid_data |= (1ULL << (63 - i));
    }

    // BCH纠错
    *valid_data = bch_decode(*valid_data);
    return 0;
}

// 核心检测函数（无ID先验）
int watermark_detect(uint8_t *rgb_img, int img_w, int img_h,
                     uint32_t *device_id, uint32_t *timestamp, char *time_str) {
    if (!rgb_img || !device_id || !timestamp || !time_str) return -1;
    *device_id = 0; *timestamp = 0;
    memset(time_str, 0, 20);

    // 1. 读取配置
    WatermarkConfig config;
    if (read_watermark_config(WATERMARK_CONFIG_PATH, &config) < 0) return -1;
    TEMPLATE_W = config.template_width;
    TEMPLATE_H = config.template_height;
    PIXEL_SPACING = config.pixel_spacing;
    REDUNDANCY = config.template_redundancy;

    // 2. RGB转Y通道+边缘增强
    uint8_t *y_channel = (uint8_t *)malloc(img_w * img_h);
    if (!y_channel) return -1;
    rgb2y_channel(rgb_img, y_channel, img_w, img_h);

    // 边缘增强（抵消模糊）
    for (int y = 1; y < img_h - 1; y++) {
        for (int x = 1; x < img_w - 1; x++) {
            int pos = y * img_w + x;
            uint8_t val = 5 * y_channel[pos] - y_channel[pos-1] - y_channel[pos+1] - y_channel[pos-img_w] - y_channel[pos+img_w];
            y_channel[pos] = val > 255 ? 255 : (val < 0 ? 0 : val);
        }
    }

    // 3. 定位水印区
    int match_x[3], match_y[3];
    float match_scores[3];
    int valid_regions = find_watermark_regions(y_channel, img_w, img_h,
                                              TEMPLATE_W, TEMPLATE_H,
                                              REDUNDANCY,
                                              match_x, match_y, match_scores);
    if (valid_regions < 1) {
        free(y_channel);
        return -1;
    }

    // 4. 采样多组二进制串
    uint64_t binary_arr[3] = {0};
    for (int i = 0; i < valid_regions; i++) {
        sample_dot_matrix(y_channel, img_w, img_h,
                         match_x[i], match_y[i],
                         TEMPLATE_W, TEMPLATE_H,
                         &binary_arr[i]);
    }

    // 5. 投票+纠错
    uint64_t valid_64bit;
    if (vote_and_correct(binary_arr, valid_regions, &valid_64bit) < 0) {
        free(y_channel);
        return -1;
    }

    // 6. 拆分ID+时间戳
    *device_id = (uint32_t)((valid_64bit >> 32) & 0xFFFFFFFF);
    *timestamp = (uint32_t)(valid_64bit & 0xFFFFFFFF);

    // 7. 时间戳验证+格式化
    time_t curr = time(NULL);
    if (*timestamp > curr || *timestamp < 1609459200) {
        fprintf(stderr, "Invalid timestamp: %u\n", *timestamp);
    }
    struct tm *tm = gmtime((time_t *)timestamp);
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", tm);

    free(y_channel);
    return 0;
}

// 测试主函数（支持OpenCV/二进制文件）
#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <screenshot.jpg/png>\n", argv[0]);
        return -1;
    }

    cv::Mat img = cv::imread(argv[1]);
    if (img.empty()) return -1;

    cv::Mat rgb;
    cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);
    uint32_t dev_id, ts;
    char time_str[20];

    if (watermark_detect(rgb.data, rgb.cols, rgb.rows, &dev_id, &ts, time_str) == 0) {
        printf("===== Detection Result =====\n");
        printf("Device ID: 0x%08X\n", dev_id);
        printf("Capture Time (UTC): %s\n", time_str);
        printf("============================\n");
    } else {
        printf("No valid watermark detected\n");
        return -1;
    }

    return 0;
}
#else
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <rgb.bin> <width> <height>\n", argv[0]);
        return -1;
    }

    int w = atoi(argv[2]), h = atoi(argv[3]);
    uint8_t *rgb = (uint8_t *)malloc(w * h * 3);
    FILE *fp = fopen(argv[1], "rb");
    if (!fp || !rgb) return -1;
    fread(rgb, 1, w * h * 3, fp);
    fclose(fp);

    uint32_t dev_id, ts;
    char time_str[20];
    if (watermark_detect(rgb, w, h, &dev_id, &ts, time_str) == 0) {
        printf("===== Detection Result =====\n");
        printf("Device ID: 0x%08X\n", dev_id);
        printf("Capture Time (UTC): %s\n", time_str);
        printf("============================\n");
    } else {
        printf("No valid watermark detected\n");
        free(rgb);
        return -1;
    }

    free(rgb);
    return 0;
}
#endif