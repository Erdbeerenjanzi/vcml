/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BITOPS_H
#define VCML_BITOPS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"

namespace vcml {

    inline int clz(u32 val) {
        return val ? __builtin_clz(val) : 32;
    }

    inline int clz(u64 val) {
        return val ? __builtin_clzl(val) : 64;
    }

    inline int ctz(u32 val) {
        return val ? __builtin_ctz(val) : 32;
    }

    inline int ctz(u64 val) {
        return val ? __builtin_ctzl(val) : 64;
    }

    inline int ffs(u32 val) {
        return __builtin_ffs(val) - 1;
    }

    inline int ffs(u64 val) {
        return __builtin_ffsl(val) - 1;
    }

    template <typename T>
    inline unsigned int fls(T val) {
        return sizeof(T) * 8 - clz(val) - 1;
    }

    template <typename T>
    inline unsigned int popcnt(T val) {
        return __builtin_popcountl((long)val);
    }

    template <typename T>
    inline bool is_pow2(T val) {
        return val != 0 && popcnt(val) == 1;
    }

    template <typename T>
    inline T extract(T val, unsigned int off, unsigned int len) {
        return (val >> off) & ((1ull << len) - 1);
    }

    template <typename T, typename T2>
    static inline T deposit(T val, unsigned int off, unsigned int len, T2 x) {
        const T mask = ((1ull << len) - 1) << off;
        return (val & ~mask) | (((T)x << off) & mask);
    }

    // crc7 calculates a 7 bit CRC of the specified data using the polynomial
    // x^7 + x^3 + 1. It will be stored in the upper 7 bits of the result.
    extern const u8 crc7_table[256]; //
    inline u8 crc7(const u8* buffer, size_t len) {
        u8 crc = 0;
        while (len--)
            crc = crc7_table[crc ^ *buffer++];
        return crc;
    }

    // crc16 calculates a 16 bit CRC of the given data using the polynomial
    // // x^16 + x^12 + x^5 + 1.
    extern const u16 crc16_table[256];
    inline u16 crc16(const u8* buffer, size_t len) {
        u16 crc = 0;
        while (len--)
            crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *buffer++) & 0xff];
        return crc;
    }

    template <unsigned int OFF, unsigned int LEN, typename T = u64>
    struct bitfield {
        enum : unsigned int { OFFSET = OFF };
        enum : unsigned int { LENGTH = LEN };
        enum : T { MASK = ((1ull << LEN) - 1) << OFF };

        operator T() const { return MASK; }
    };

    template <typename F, typename T>
    T get_bitfield(F f, T val) {
        return extract(val, F::OFFSET, F::LENGTH);
    }

    template <typename F, typename T, typename T2>
    void set_bitfield(F f, T& val, T2 x) {
        val = deposit(val, F::OFFSET, F::LENGTH, x);
    }

}

#endif
