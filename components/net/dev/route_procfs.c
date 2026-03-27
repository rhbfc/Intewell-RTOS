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

#include <assert.h>
#include <errno.h>
#include <fs/procfs.h>
#include <net/netdev.h>
#include <net/netdev_route.h>
#include <spinlock.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ttos_init.h>

#define KLOG_TAG "ROUTE_PROCFS"
#include <klog.h>

static int proc_open(struct proc_dir_entry *inode, void *buffer);
static int proc_release(struct proc_dir_entry *inode, void *buffer);
static ssize_t route_proc_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                               off_t *ppos);

struct seq_data
{
    char buf[4096];
    struct proc_seq_buf seq;
};

/* 序列化文件操作结构体 */
static struct proc_ops route_ops = {
    .proc_open = proc_open, .proc_read = route_proc_read, .proc_release = proc_release};

/**
 * Functions
 */

static int route_proc_init()
{
    proc_create("net/route", 0444, NULL, &route_ops);
    return 0;
}
INIT_EXPORT_SERVE_FS(route_proc_init, "init netdev procfs");

static char ROUTE_TABLE_HEAD[] = "Iface\t"
                                 "Destination\t"
                                 "Gateway\t"
                                 "Flags\t"
                                 "RefCnt\t"
                                 "Use\t"
                                 "Metric\t"
                                 "Mask\t"
                                 "MTU\t"
                                 "Window\t"
                                 "IRTT\n";

static int fill_route_info(struct proc_seq_buf *seq)
{
    long flags;
    static_route_item_t *rti;

    proc_seq_printf(seq, ROUTE_TABLE_HEAD);

    spin_lock_irqsave(&STATIC_ROUTE_LOCK, flags);

    list_for_each_entry(rti, &STATIC_ROUTE_LIST, node)
    {
        /* Iface  Destination  Gateway  Flags  *  *  *  Mask  *  *  * */
        proc_seq_printf(seq, "%s\t%x\t%x\t%x\t0\t0\t0\t%x\t0\t0\t0\n", rti->ndev->name,
                        rti->dest.s_addr, rti->gateway.s_addr, rti->flags, rti->netmask.s_addr);
    }

    spin_unlock_irqrestore(&STATIC_ROUTE_LOCK, flags);

    return 0;
}

static int proc_open(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = calloc(1, sizeof(struct seq_data));
    if (!data)
    {
        return -ENOMEM;
    }
    *(struct seq_data **)buffer = data;
    return 0;

    return 0;
}

static int proc_release(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = buffer;
    if (data)
    {
        free(data);
    }
    return 0;
}

static ssize_t route_proc_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                               off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        fill_route_info(&data->seq);
    }
    /* 使用proc_seq_read读取数据 */
    return proc_seq_read(&data->seq, buffer, count);
}
