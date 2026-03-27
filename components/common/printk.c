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

/*
 * @file: printk.c
 * @brief:
 *    <li>内核打印函数</li>
 */

/************************头 文 件******************************/
#include <limits.h>
#include <spinlock.h>
#include <stdarg.h>
#include <stdbool.h>
#define __DEFINED_struct__IO_FILE 1
#include <stdio.h>
#include <stdlib.h>
#include <system/types.h>

/************************宏 定 义******************************/

/************************类型定义******************************/
typedef void (*outputfunc_ptr)(char ch);

struct _IO_FILE
{
    unsigned flags;
    unsigned char *rpos, *rend;
    int (*close)(FILE *);
    unsigned char *wend, *wpos;
    unsigned char *mustbezero_1;
    unsigned char *wbase;
    size_t (*read)(FILE *, unsigned char *, size_t);
    size_t (*write)(FILE *, const unsigned char *, size_t);
    off_t (*seek)(FILE *, off_t, int);
    unsigned char *buf;
    size_t buf_size;
    FILE *prev, *next;
    int fd;
    int pipe_pid;
    long lockcount;
    int mode;
    volatile int lock;
    int lbf;
    void *cookie;
    off_t off;
    char *getln_buf;
    void *mustbezero_2;
    unsigned char *shend;
    off_t shlim, shcnt;
    FILE *prev_locked, *next_locked;
    struct __locale_struct *locale;
};

/************************外部声明******************************/

/************************前向声明******************************/
static void outputChar_def(char ch);

/************************模块变量******************************/
static outputfunc_ptr outputChar = outputChar_def;

/* 定义打印锁 */
static DEFINE_SPINLOCK(printk_lock);

/************************全局变量******************************/

/************************函数实现******************************/

static void outputChar_def(char ch) {}
/**
 * @brief:
 *    初始化打印单个字符的函数指针。
 * @param[in]: printChar: 单个字符的函数指针
 * @return:
 *    无
 */
void printk_init(void *printChar)
{
    outputChar = (outputfunc_ptr)printChar;
}

/**
 * @brief:
 *    打印字符串。
 * @param[in]: buf: 打印缓冲
 * @param[in]: size: 打印数据数量
 * @return:
 *    无
 */
void print_buf(char *buf, u32 size)
{
    u32 i = 0;

    for (i = 0; i < size; i++)
    {
        if (buf[i] == '\n')
        {
            outputChar('\r');
        }
        outputChar(buf[i]);
    }
}

static size_t early_printk_write(FILE *f, const unsigned char *s, size_t l)
{
    size_t k = f->wpos - f->wbase;
    if (k)
    {
        print_buf((char *)f->wbase, k);
    }
    k = l;
    if (k)
    {
        print_buf((char *)s, k);
    }
    f->wpos = f->wbase = f->buf;
    /* pretend to succeed, even if we discarded extra data */
    return l;
}

int vprintk(const char *fmt, va_list ap)
{
    long flags;
    int ret;
    unsigned char buf[1];
    FILE f = {
        .lbf = EOF,
        .write = early_printk_write,
        .lock = -1,
        .buf = buf,
        .cookie = NULL,
    };

    /* 获取打印自旋锁 */
    spin_lock_irqsave(&printk_lock, flags);

    /* 解析并打印字符串 */
    ret = vfprintf(&f, fmt, ap);

    /* 释放打印自旋锁 */
    spin_unlock_irqrestore(&printk_lock, flags);
    return ret;
}
/*
 * @brief
 *    按照指定接口进行打印。
 * @param[in] fmt: 打印格式
 * @param[in] ...: 可变参
 * @return
 *     none
 */
int printk(const char *fmt, ...)
{
    va_list ap;
    int ret;

    /* 开始可变参数读取 */
    va_start(ap, fmt);

    ret = vprintk(fmt, ap);

    /* 结束可变参数读取 */
    va_end(ap);
    return ret;
}
