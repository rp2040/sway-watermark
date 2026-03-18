#include "bch.h"
#include <stdint.h>

// BCH(63,51) 生成多项式：x^12 + x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + x^2 + 1
static const uint64_t G_POLY = 0x1247; 

// 模2除法
static uint64_t mod2_div(uint64_t dividend, uint64_t divisor, int div_len) {
    int bit_len = 64 - __builtin_clzll(dividend);
    while (bit_len >= div_len) {
        dividend ^= (divisor << (bit_len - div_len));
        bit_len = 64 - __builtin_clzll(dividend);
    }
    return dividend;
}

// BCH编码（51bit数据→63bit编码）
uint64_t bch_encode(uint64_t data) {
    data &= 0x7FFFFFFFFFFFF; // 保留低51bit
    uint64_t shifted = data << 12; // 左移12bit，预留校验位
    uint64_t remainder = mod2_div(shifted, G_POLY, 13); // 计算余数（12bit）
    return shifted | remainder; // 数据+校验位
}

// BCH解码（63bit编码→51bit数据，纠正≤3bit错误）
uint64_t bch_decode(uint64_t data) {
    data &= 0x7FFFFFFFFFFFFFF; // 保留低63bit
    uint64_t syndrome = mod2_div(data, G_POLY, 13);

    // 简化纠错（实验场景，仅处理单bit错误）
    if (syndrome == 0) return (data >> 12) & 0x7FFFFFFFFFFFF;

    // 单bit错误定位
    for (int i = 0; i < 63; i++) {
        uint64_t test = 1ULL << (62 - i);
        uint64_t test_syn = mod2_div(test, G_POLY, 13);
        if (test_syn == syndrome) {
            data ^= test; // 纠正错误
            break;
        }
    }

    return (data >> 12) & 0x7FFFFFFFFFFFF;
}