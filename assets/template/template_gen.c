#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// 生成固定模板（替代Python脚本）
int generate_fixed_template(uint32_t device_id, uint32_t timestamp, 
                           const char *output_path, int template_w, int template_h) {
    int template_size = template_w * template_h;
    uint8_t *template_data = (uint8_t *)malloc(template_size);
    if (!template_data) return -1;
    memset(template_data, 0, template_size);

    // 拼接64bit串
    uint64_t watermark_data = ((uint64_t)device_id << 32) | (uint64_t)timestamp;

    // 伪随机位置（设备ID为种子）
    srand(device_id);
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
        positions[i][0] = pos / template_w;
        positions[i][1] = pos % template_w;
    }

    // 赋值
    for (int i = 0; i < 64; i++) {
        int y = positions[i][0], x = positions[i][1];
        uint64_t bit = (watermark_data >> (63 - i)) & 1;
        template_data[y * template_w + x] = bit ? 255 : 0;
    }

    // 写入文件
    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        free(template_data);
        return -1;
    }
    fwrite(template_data, 1, template_size, fp);
    fclose(fp);
    free(template_data);

    printf("Template generated: %s\n", output_path);
    printf("Device ID: 0x%08X, Timestamp: %u\n", device_id, timestamp);
    return 0;
}

// 命令行调用
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <device_id_hex> <timestamp> <output_path>\n", argv[0]);
        fprintf(stderr, "Example: %s 0x1A2B3C4D 1718000000 ./watermark_template.bin\n", argv[0]);
        return -1;
    }

    uint32_t device_id;
    if (strstr(argv[1], "0x") == argv[1]) sscanf(argv[1], "0x%X", &device_id);
    else device_id = atoi(argv[1]);
    uint32_t timestamp = atoi(argv[2]);
    const char *output_path = argv[3];

    return generate_fixed_template(device_id, timestamp, output_path, 400, 200);
}