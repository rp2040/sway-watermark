#ifndef BCH_H
#define BCH_H

#include <stdint.h>

// BCH(63,51)纠错编码（可纠正≤3bit错误）
uint64_t bch_encode(uint64_t data);
uint64_t bch_decode(uint64_t data);

#endif