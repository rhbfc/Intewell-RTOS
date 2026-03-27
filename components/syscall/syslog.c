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

#include "syscall_internal.h"
#include <errno.h>
#include <klog.h>
#include <uaccess.h>

/* 关闭日志。当前为空操作。 */
#define SYSLOG_ACTION_CLOSE 0
/* 打开日志。当前为空操作。 */
#define SYSLOG_ACTION_OPEN 1
/**
 * 从日志中读取。该调用等待直到内核日志缓冲区非空，然后最多读取 len 字节到 bufp 指向的缓冲区中。
 * 该调用返回读取的字节数。从日志中读取的字节会从日志缓冲区中消失：信息只能读取一次。
 * 这是当用户程序读取 /proc/kmsg 时内核执行的函数。
 */
#define SYSLOG_ACTION_READ 2

/**
 * 读取环形缓冲区中剩余的所有消息，将它们放入 bufp 指向的缓冲区中。
 * 该调用从日志缓冲区中读取最后 len 字节（非破坏性），但不会读取超过自上次"清除环形缓冲区"
 * 命令（见下面的命令 5）以来写入缓冲区的内容。该调用返回读取的字节数。
 */
#define SYSLOG_ACTION_READ_ALL 3

/**
 * 读取并清除环形缓冲区中剩余的所有消息。该调用执行与类型 3 完全相同的操作，
 * 但同时执行"清除环形缓冲区"命令。
 */
#define SYSLOG_ACTION_READ_CLEAR 4

/**
 * 该调用仅执行"清除环形缓冲区"命令。bufp 和 len 参数被忽略。
 * 此命令实际上并不清除环形缓冲区。相反，它设置一个内核记录变量，
 * 该变量决定命令 3(SYSLOG_ACTION_READ_ALL) 和 4(SYSLOG_ACTION_READ_CLEAR) 返回的结果。
 * 此命令对命令 2(SYSLOG_ACTION_READ) 和 9(SYSLOG_ACTION_SIZE_UNREAD) 没有影响。
 */
#define SYSLOG_ACTION_CLEAR 5

/**
 * 禁用 printk 到控制台。该调用将控制台日志级别设置为最小值，
 * 以便没有消息打印到控制台。bufp 和 len 参数被忽略。
 */
#define SYSLOG_ACTION_CONSOLE_OFF 6

/**
 * 该调用将控制台日志级别设置为默认值，以便消息打印到控制台。
 * bufp 和 len 参数被忽略。
 */
#define SYSLOG_ACTION_CONSOLE_ON 7

/**
 * 该调用将控制台日志级别设置为 len 中给定的值，该值必须是 1 到 8（包含）之间的整数。
 * 有关详细信息，请参阅日志级别部分。bufp 参数被忽略。
 */
#define SYSLOG_ACTION_CONSOLE_LEVEL 8

/**
 * 该调用返回当前可通过命令 2(SYSLOG_ACTION_READ) 从内核日志缓冲区读取的字节数。
 * bufp 和 len 参数被忽略。
 */
#define SYSLOG_ACTION_SIZE_UNREAD 9

/**
 * 此命令返回内核日志缓冲区的总大小。bufp 和 len 参数被忽略。
 */
#define SYSLOG_ACTION_SIZE_BUFFER 10

/**
 * @brief 系统调用实现：内核日志获取
 *
 * 该函数实现了一个系统调用，用于获取内核日志。
 *
 * @param[in] type type 参数决定此函数执行的操作。下面的列表指定了 type 的值。
 * 符号名称在内核源代码中定义，但不会导出到用户空间；您需要使用数字，或者自己定义名称。
 * @param[in] buf buf 参数用于向内核传递数据和从内核接收数据。
 * @param[in] len len 参数用于向内核传递数据和从内核接收数据。
 * @return 对于 type 等于 2、3 或 4，成功调用 syslog() 返回读取的字节数。
 * 对于 type 9，syslog() 返回当前可从内核日志缓冲区读取的字节数。
 * 对于 type 10，syslog() 返回内核日志缓冲区的总大小。
 * 对于其他 type 值，成功时返回 0。
 * @retval -EINVAL 错误的参数（例如，错误的 type；或对于 type 2、3 或 4，buf 为 NULL，
 * 或 len 小于零；或对于 type 8，级别超出 1 到 8 的范围）。
 * @retval -EFAULT 参数指向无效内存。
 * @retval -EINTR 系统调用被信号中断；没有读取任何内容。（这只能在跟踪期间看到。）
 *
 */
DEFINE_SYSCALL(syslog, (int type, char __user *buf, int len))
{
    int ret = 0;
    switch (type)
    {
    case SYSLOG_ACTION_CLOSE:
    case SYSLOG_ACTION_OPEN:
        ret = 0;
        break;
    case SYSLOG_ACTION_READ:
        if (buf == NULL || len < 0)
        {
            return -EINVAL;
        }
        ret = klog_read_from_klogctl(buf, len, true);
        break;
    case SYSLOG_ACTION_READ_ALL:
        if (buf == NULL || len < 0)
        {
            return -EINVAL;
        }
        ret = klog_read_from_klogctl(buf, len, false);
        break;
    case SYSLOG_ACTION_READ_CLEAR:
        if (buf == NULL || len < 0)
        {
            return -EINVAL;
        }
        ret = klog_read_from_klogctl(buf, len, true);
        break;
    case SYSLOG_ACTION_CLEAR:
        ret = klog_clear();
        break;
    case SYSLOG_ACTION_CONSOLE_OFF:
        ret = klog_level_set(KLOG_EMER);
        break;
    case SYSLOG_ACTION_CONSOLE_ON:
        ret = klog_level_set(KLOG_LEVEL);
        break;
    case SYSLOG_ACTION_CONSOLE_LEVEL:
        ret = klog_level_set(len);
        break;
    case SYSLOG_ACTION_SIZE_UNREAD:
        return klog_get_size();
    case SYSLOG_ACTION_SIZE_BUFFER:
        return CONFIG_KLOG_BUFF_SIZE;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}