/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __KLOG_H__
#define __KLOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KLOG_TAG
#define KLOG_TAG "NOTAG"
#endif

#include <stdbool.h>
#include <stddef.h>

enum klog_level
{
    KLOG_EMER = 0, /* system is unusable               */
    KLOG_ALERT,    /* action must be taken immediately */
    KLOG_CRIT,
    KLOG_ERR,
    KLOG_WARN,
    KLOG_NOTICE,
    KLOG_INFO,
    KLOG_DEBUG,
    KLOG_END,
    KLOG_MAX = 0xFFFFFFFF,
};

#ifndef KLOG_LEVEL
#ifdef CONFIG_LOG_LEVEL_DEBUG
#define KLOG_LEVEL KLOG_DEBUG
#elif defined(CONFIG_LOG_LEVEL_INFO)
#define KLOG_LEVEL KLOG_INFO
#elif defined(CONFIG_LOG_LEVEL_WARN)
#define KLOG_LEVEL KLOG_WARN
#elif defined(CONFIG_LOG_LEVEL_ERROR)
#define KLOG_LEVEL KLOG_ERR
#elif defined(CONFIG_LOG_LEVEL_EMERG)
#define KLOG_LEVEL KLOG_EMER
#else
#define KLOG_LEVEL KLOG_EMER
#endif
#endif

#define KLOG_MAGIC 0x6b6c6f67 // klog对应的ascii

struct klog_record
{
    unsigned int magic;
    int klog_level;
    long ts_sec;
    long ts_nsec;
    unsigned int seq;
    int buf_size;
};

void klog_print(int log_level, int is_buf, const char *tag, const char *fmt, ...);

#define KLOG_X(level, ...)                                                                         \
    do                                                                                             \
    {                                                                                              \
        klog_print(level, 1, KLOG_TAG, __VA_ARGS__);                                               \
    } while (0)

/* B12 used */
// #define KLOG_X(level, ...)                          \
//     do{                                             \
//         klog_print(level, 1, KLOG_EMERG == level,   \
//                     KLOG_TAG, __VA_ARGS__);         \
//     } while (0)

#define KLOG_D(arg...) KLOG_X(KLOG_DEBUG, arg)
#define KLOG_I(arg...) KLOG_X(KLOG_INFO, arg)
#define KLOG_W(arg...) KLOG_X(KLOG_WARN, arg)
#define KLOG_E(arg...) KLOG_X(KLOG_ERR, arg)
#define KLOG_EMERG(arg...) KLOG_X(KLOG_EMER, arg)

int klog_read_circbuf(char *buf, int pos, int len);
int klog_read_msg(char *buffer, unsigned int *pos, unsigned int buflen, bool block);
int klog_read_from_klogctl(char *buffer, unsigned int buflen, bool clear);
size_t klog_get_size(void);
int klog_level_set(int level);
int klog_level_get(void);
int klog_clear(void);

#define DUMP_PREFIX_NONE 0    // 不加任何前缀
#define DUMP_PREFIX_ADDRESS 1 // 每行前缀加地址（buf 中相对地址）
#define DUMP_PREFIX_OFFSET 2  // 每行前缀加偏移量

void log_print_hex_dump(int level, const char *prefix_str, int prefix_type, int rowsize,
                        int groupsize, const void *buf, size_t len, bool ascii);
/* 兼容linux */
#define print_hex_dump(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_DEBUG, a, b, c, d, e, f, g)

#define KLOG_HEX_D(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_DEBUG, a, b, c, d, e, f, g)
#define KLOG_HEX_I(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_INFO, a, b, c, d, e, f, g)
#define KLOG_HEX_W(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_WARN, a, b, c, d, e, f, g)
#define KLOG_HEX_E(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_ERR, a, b, c, d, e, f, g)
#define KLOG_HEX_EMERG(a, b, c, d, e, f, g) log_print_hex_dump(KLOG_EMER, a, b, c, d, e, f, g)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __KLOG_H__ */
