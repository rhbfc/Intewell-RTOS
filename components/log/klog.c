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

#include <circbuf.h>
#include <commonUtils.h>
#include <inttypes.h>
#include <klog.h>
#include <linux/wait.h>
#include <mmu.h>
#include <page.h>
#include <spinlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <time/ktime.h>
#include <trace.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <unaligned.h>

#ifndef CONFIG_KLOG_BUFF_SIZE
#define KLOG_BUFF_SIZE (16 * 1024 * 1024)
#else
#define KLOG_BUFF_SIZE CONFIG_KLOG_BUFF_SIZE
#endif

/* CSI(Control Sequence Introducer/Initiaor) */
#define CSI_START "\033["
#define CSI_END "\033[0m"

/* Front color */
#define F_BLACK "30;"
#define F_RED "31;"
#define F_GREEN "32;"
#define F_YELLOW "33;"
#define F_BLUE "34;"
#define F_MAGENTA "35;"
#define F_CYAN "36;"
#define F_WHITE "37;"

/* Backeground color */
#define B_NULL
#define B_BLACK "40;"
#define B_RED "41;"
#define B_GREEN "42;"
#define B_YELLOW "43;"
#define B_BLUE "44;"
#define B_MAGENTA "45;"
#define B_CYAN "46;"
#define B_WHITE "47;"

/* Front style */
#define S_BOLD "1m"
#define S_UNDERLINE "4m"
#define S_BLINK "5m"
#define S_NORMAL "22m"

/* style = front color + background color + front style */
#define KLOG_DEBUG_STYLE (CSI_START F_WHITE B_NULL S_NORMAL)
#define KLOG_INFO_STYLE (CSI_START F_BLUE B_NULL S_NORMAL)
#define KLOG_WARN_STYLE (CSI_START F_YELLOW B_NULL S_BOLD)
#define KLOG_ERROR_STYLE (CSI_START F_RED B_NULL S_BOLD)

static circbuf_t g_klog_buf;
static bool klog_inited = false;
static DEFINE_SPINLOCK(g_klog_lock);
#define KLOG_LOCK(flags) spin_lock_irqsave(&g_klog_lock, flags)
#define KLOG_UNLOCK(flags) spin_unlock_irqrestore(&g_klog_lock, flags)

static wait_queue_head_t g_klog_wq;

static int g_klog_level = KLOG_LEVEL;

static char level_color[][10] = {
    [0 ... KLOG_END] = "",           [KLOG_EMER] = KLOG_ERROR_STYLE,
    [KLOG_ALERT] = KLOG_ERROR_STYLE, [KLOG_CRIT] = KLOG_ERROR_STYLE,
    [KLOG_ERR] = KLOG_ERROR_STYLE,   [KLOG_WARN] = KLOG_WARN_STYLE,
    [KLOG_NOTICE] = KLOG_INFO_STYLE, [KLOG_INFO] = KLOG_INFO_STYLE,
    [KLOG_DEBUG] = KLOG_DEBUG_STYLE,
};

static char level_tag[][10] = {
    [0 ... KLOG_END] = "", [KLOG_EMER] = "EMERG", [KLOG_ALERT] = "ALERT",
    [KLOG_CRIT] = "CRIT",  [KLOG_ERR] = "E",      [KLOG_WARN] = "W",
    [KLOG_NOTICE] = "N",   [KLOG_INFO] = "I",     [KLOG_DEBUG] = "D",
};

static void klog_init(void)
{
    if (klog_inited)
    {
        return;
    }
    init_waitqueue_head(&g_klog_wq);
    circbuf_init(&g_klog_buf,
                 (void *)page_address(pages_alloc(page_bits(KLOG_BUFF_SIZE), ZONE_NORMAL)),
                 KLOG_BUFF_SIZE);
    klog_inited = true;
}

int klog_level_set(int level)
{
    if (level > KLOG_END || level < KLOG_ALERT)
    {
        return -EINVAL;
    }
    g_klog_level = level;
    return 0;
}

int klog_level_get(void)
{
    return g_klog_level;
}

void klog_print(int log_level, int is_buf, const char *tag, const char *fmt, ...)
{
    struct klog_record r;
    struct timespec tp;
    long flags;
    int output_now = g_klog_level >= log_level;
    static unsigned int seq = 0;

    r.magic = KLOG_MAGIC;
    r.klog_level = log_level;
    r.seq = seq++;

    int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (ret < 0)
    {
        r.ts_sec = r.ts_sec = 0;
    }
    else
    {
        r.ts_sec = tp.tv_sec;
        r.ts_nsec = tp.tv_nsec;
    }

    char buffer[1024];
    char *tmp = buffer;
    ret = sprintf(tmp, "%s/%s: ", level_tag[log_level], tag);

    tmp += ret;

    va_list va;
    if (page_allocer_inited() && is_buf)
    {
        klog_init();
    }

    va_start(va, fmt);
    int len = vsnprintf(tmp, sizeof(buffer) - ret, fmt, va);
    va_end(va);

    trace_log(log_level, (char *)tag, (char *)tmp, len + strlen(tag));

    r.buf_size = (ret + len);

    if (page_allocer_inited() && is_buf)
    {
        KLOG_LOCK(flags);

        /* write struct klog */
        circbuf_overwrite(&g_klog_buf, &r, sizeof(struct klog_record));

        /* write log string */
        circbuf_overwrite(&g_klog_buf, buffer, ret + len);

        KLOG_UNLOCK(flags);
    }

    if (!page_allocer_inited() || output_now)
    {
        printk("[%6ld.%06ld] ", r.ts_sec, r.ts_nsec / 1000UL);
        printk("%s", level_color[log_level]);
        printk("%s" CSI_END "\n", buffer);
    }

    /* 这里必须要判断，否则从核刚启动时还未初始化调度器调用printk会导致异常 */
    if (TTOS_GetRunningTask() && log_level != KLOG_EMER)
    {
        wake_up_interruptible(&g_klog_wq);
    }
}

int klog_read_circbuf(char *buf, int pos, int len)
{
    long flags;
    KLOG_LOCK(flags);
    int ret = circbuf_peekat(&g_klog_buf, g_klog_buf.tail + pos, buf, len);
    KLOG_UNLOCK(flags);
    return ret;
}

static int klog_read_circbuf_locked(char *buf, int pos, int len)
{
    /*
     * klog_read_a_msg_raw() 已经持有 g_klog_lock，不能再走
     * klog_read_circbuf()，否则在非递归 spinlock 配置下会二次加锁死锁。
     */
    return circbuf_peekat(&g_klog_buf, g_klog_buf.tail + pos, buf, len);
}

size_t klog_get_size(void)
{
    size_t size;
    long flags;
    KLOG_LOCK(flags);
    size = circbuf_used(&g_klog_buf);
    KLOG_UNLOCK(flags);
    return size;
}

int klog_clear(void)
{
    long flags;
    KLOG_LOCK(flags);
    circbuf_reset(&g_klog_buf);
    KLOG_UNLOCK(flags);
    return 0;
}

static size_t klog_read_a_msg_raw(unsigned int *pos, long *sec, long *nsec, int *level,
                                  unsigned int *seq, char *buf, size_t len, bool clear)
{
    struct klog_record r;
    size_t size = 0;
    long flags;

    if (sec == NULL || nsec == NULL || level == NULL || seq == NULL || buf == NULL)
    {
        return 0;
    }

    /* 如果是清除读取 则只能从头开始读 */
    if (clear && (*pos != 0))
    {
        return 0;
    }

    KLOG_LOCK(flags);

    /* 已持有 g_klog_lock，后续读取必须使用 *_locked 版本避免重入加锁 */
    for (;;)
    {
        size = klog_read_circbuf_locked((void *)&r, *pos, sizeof(struct klog_record));
        if (size != sizeof(struct klog_record))
        {
            if (clear)
            {
                circbuf_skip(&g_klog_buf, size);
            }
            break;
        }

        /* 处理ringbuf被覆盖的情况 */
        if (r.magic != KLOG_MAGIC)
        {
            if (clear)
            {
                circbuf_skip(&g_klog_buf, 1);
            }
            else
            {
                *pos += 1;
            }

            continue;
        }

        break;
    }

    if (size != sizeof(struct klog_record))
    {
        size = 0;
        goto out;
    }
    if (clear)
    {
        circbuf_skip(&g_klog_buf, size);
    }
    else
    {
        *pos += size;
    }

    *sec = r.ts_sec;
    *nsec = r.ts_nsec;
    *level = r.klog_level;
    *seq = r.seq;

    size = (len - 1) < r.buf_size ? (len - 1) : r.buf_size;

    size = klog_read_circbuf_locked(buf, *pos, size);
    buf[size] = '\0';
    if (clear)
    {
        circbuf_skip(&g_klog_buf, r.buf_size);
    }
    else
    {
        *pos += r.buf_size;
    }
out:
    KLOG_UNLOCK(flags);
    return size;
}
// todo 暂时无法解决读不完一整条信息的情况，会导致某条日志丢失一部分
int klog_read_msg(char *buffer, unsigned int *pos, unsigned int buflen, bool block)
{
    size_t size = 0;
    size_t len;
    long sec, nsec;
    int level;
    int ret = 0;
    unsigned int seq;
    char buf[1024];
    unsigned int ret_len = 0;

    if (block)
    {
        ret =
            wait_event_interruptible(&g_klog_wq, klog_read_a_msg_raw(pos, &sec, &nsec, &level, &seq,
                                                                     buf, sizeof(buf), false) != 0);
        if (ret < 0)
        {
            return ret;
        }
    }
    else
    {
        size = klog_read_a_msg_raw(pos, &sec, &nsec, &level, &seq, buf, sizeof(buf), false);
        if (size == 0)
        {
            return 0;
        }
    }

    ret_len = snprintf(buffer, buflen, "%d,%d,%" PRId64 ",-;%s\n", level, seq,
                       (uint64_t)(sec * USEC_PER_SEC + nsec / NSEC_PER_USEC), buf);
    return (ret_len > buflen) ? buflen : ret_len;
}

int klog_read_from_klogctl(char *buffer, unsigned int buflen, bool clear)
{
    int ret;
    size_t read_size = 0;
    size_t len;
    size_t size = 0;
    long sec, nsec;
    int level;
    unsigned int seq;
    char buf[1024];
    unsigned int pos = 0;
    while (read_size < buflen)
    {
        /* 当buffer读空后，如果存在读取了部分，则应返回以处理读取的部分，否则应挂起等待新的数据写入
         */
        ret = wait_event_interruptible(
            &g_klog_wq, ((size = klog_read_a_msg_raw(&pos, &sec, &nsec, &level, &seq, buf,
                                                     sizeof(buf), clear)) != 0) ||
                            read_size != 0);
        if (ret < 0)
        {
            return ret;
        }

        if (size == 0)
        {
            return read_size;
        }
        read_size += snprintf(buffer + read_size, buflen - read_size,
                              "<%d>[%6" PRIdPTR ".%06" PRId64 "] %s\n", level, sec,
                              (uint64_t)nsec / NSEC_PER_USEC, buf);
    }
    return read_size;
}

static void klog_output(int fd, size_t buf_size)
{
    char buffer[256];
    size_t len = 0;
    size_t read_size = 0;
    uint32_t pos = 0;

    while (pos < buf_size)
    {
        read_size = buf_size - pos;
        read_size = read_size > sizeof(buffer) ? sizeof(buffer) : read_size;
        len = klog_read_msg(buffer, &pos, read_size, false);

        if (len > 0)
            write(fd, buffer, len);

        if (!len)
            break;
    }
}

#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
void log_print_hex_dump(int level, const char *prefix_str, int prefix_type, int rowsize,
                        int groupsize, const void *buf, size_t len, bool ascii)
{
    unsigned char *pbuf = (unsigned char *)buf;
    char linebuf[128];
    int pos;
    int i, j;

    if (groupsize != 1 && groupsize != 2 && groupsize != 4 && groupsize != 8)
    {
        KLOG_E("%s(%d) groupsize must be 1, 2, 4, 8 but %d", __func__, __LINE__, groupsize);
        return;
    }

    for (i = 0; i < len; i += rowsize)
    {
        switch (prefix_type)
        {
        case DUMP_PREFIX_ADDRESS:
            // 打印实际地址，并加上 prefix_str
            pos = snprintf(linebuf, sizeof(linebuf), "%s: %p: ", prefix_str, pbuf + i);
            break;
        case DUMP_PREFIX_OFFSET:
            // 打印偏移量，并加上 prefix_str
            pos = snprintf(linebuf, sizeof(linebuf), "%s: %04x: ", prefix_str, i);
            break;
        case DUMP_PREFIX_NONE:
        default:
            // 不加任何前缀，prefix_str 被完全忽略
            linebuf[0] = '\0';
            pos = 0;
            break;
        }

        for (j = 0; j < rowsize; j += groupsize)
        {
            if (i + j < len)
            {
                switch (groupsize)
                {
                case 1:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "%02x ", pbuf[i + j]);
                    break;
                case 2:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "%04x ",
                                    get_unaligned_16(pbuf + i + j));
                    break;
                case 4:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "%08x ",
                                    get_unaligned_32(pbuf + i + j));
                    break;
                case 8:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "%016llx ",
                                    get_unaligned_64(pbuf + i + j));
                    break;
                default:

                    break;
                }
            }
            else
            {
                switch (groupsize)
                {
                case 1:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "   ");
                    break;
                case 2:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "     ");
                    break;
                case 4:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "         ");
                    break;
                case 8:
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "                 ");
                    break;
                default:

                    break;
                }
            }
        }
        if (ascii)
        {
            pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "  ");

            for (j = 0; j < rowsize; j++)
            {
                if (i + j < len)
                {
                    pos += snprintf(&linebuf[pos], sizeof(linebuf) - pos, "%c",
                                    __is_print(pbuf[i + j]) ? pbuf[i + j] : '.');
                }
            }
        }

        KLOG_X(level, "%s", linebuf);
    }
}

#include <shell.h>

static int dmesg_command(int argc, const char *argv[])
{
    klog_output(STDOUT_FILENO, KLOG_BUFF_SIZE);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 dmesg, dmesg_command, show kernel log);
