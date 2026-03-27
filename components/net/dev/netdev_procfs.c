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
#include <net/ethernet_dev.h>
#include <net/netdev.h>
#include <spinlock.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ttos_init.h>

#define KLOG_TAG "NETDEV"
#include <klog.h>

/* 该宏的值应与lwipopts.h文件中的宏ARP_TABLE_SIZE保持一致 */
#define ARP_TABLE_ENTRY_NUM 128

extern int pcap_enable_flag;

static int proc_open(struct proc_dir_entry *inode, void *buffer);
static int proc_release(struct proc_dir_entry *inode, void *buffer);
static ssize_t netdev_proc_read(struct proc_dir_entry *inode, void *priv, char *buffer,
                                size_t count, off_t *ppos);
static ssize_t proc_arp_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                             off_t *ppos);
static int proc_pcap_open(struct proc_dir_entry *inode, void *buffer);
static ssize_t proc_pcap_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                             off_t *ppos);
static ssize_t proc_pcap_write(struct proc_dir_entry *entry, void *priv, const char *buffer, size_t count,
                             off_t *ppos);

struct seq_data
{
    char buf[4096];
    struct proc_seq_buf seq;
};

/* 序列化文件操作结构体 */
static struct proc_ops netdev_ops = {
    .proc_open = proc_open, .proc_read = netdev_proc_read, .proc_release = proc_release};

static struct proc_ops arp_ops = {
    .proc_open = proc_open, .proc_read = proc_arp_read, .proc_release = proc_release};

static struct proc_ops pcap_ops = {
    .proc_open = proc_pcap_open, .proc_read = proc_pcap_read, .proc_write = proc_pcap_write, .proc_release = proc_release};

/**
 * Functions
 */

static int netdev_proc_init()
{
    proc_create("net/dev", 0444, NULL, &netdev_ops);
    proc_create("net/arp", 0444, NULL, &arp_ops);
    proc_create("net/pcap", 0666, NULL, &pcap_ops);

    return 0;
}
INIT_EXPORT_SERVE_FS(netdev_proc_init, "init netdev procfs");

/**
 * 在每次pos == 0时填充当前网卡信息，按行分隔
 * 目前仅在每行显示网卡名
 */
static int fill_netdev_info(struct proc_seq_buf *seq)
{
    long flags;
    struct list_node *node;
    NET_DEV *dev;

    proc_seq_printf(seq, "| Inter- | ... \n");
    proc_seq_printf(seq, "| face   | bytes ... | bytes ... \n");

    spin_lock_irqsave(&netdev_spinlock, flags);

    for (node = &NETDEV_LIST->node; node != NULL; node = list_next(node, &(NETDEV_LIST->node)))
    {
        dev = list_entry(node, NET_DEV, node);

        if (dev->ll_type != NET_LL_ETHERNET)
        {
            continue;
        }

        proc_seq_printf(seq,
                        "%s: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " 0 0 %" PRIu64
                        " %" PRIu64 " %" PRIu64 " %" PRIu64 " 0 0\n",
                        dev->name, dev->ll_info.ethernet.pkt_stats.in_bytes, dev->ll_info.ethernet.pkt_stats.in_pacs,
                        dev->ll_info.ethernet.pkt_stats.ein_pacs, dev->ll_info.ethernet.pkt_stats.din_pacs, dev->ll_info.ethernet.pkt_stats.out_bytes,
                        dev->ll_info.ethernet.pkt_stats.out_pacs, dev->ll_info.ethernet.pkt_stats.eout_pacs,
                        dev->ll_info.ethernet.pkt_stats.dout_pacs);
    }

    spin_unlock_irqrestore(&netdev_spinlock, flags);

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

static ssize_t netdev_proc_read(struct proc_dir_entry *inode, void *priv, char *buffer,
                                size_t count, off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        /* 填充网卡信息 */
        fill_netdev_info(&data->seq);
    }
    /* 使用proc_seq_read读取数据 */
    return proc_seq_read(&data->seq, buffer, count);
}

/* 更新/proc/net/arp文件的内容 */
static int update_arp_file(struct proc_seq_buf *seq)
{
    char ip_str[16] = {0};
    char hw_str[18] = {0};
    unsigned char state;
    char dev_name[16] = {0};
    int i;
    int ret;

    proc_seq_printf(seq,
                    "%-18s"
                    "%-13s"
                    "%-13s"
                    "%-24s"
                    "%-10s"
                    "%s\n",
                    "IP address", "HW type", "Flags", "HW address", "Mask", "Device");

    for (i = 0; i < ARP_TABLE_ENTRY_NUM; i++)
    {
        ret = eth_get_arp_entry(i, (char *)&ip_str, (char *)&hw_str, &state, (char *)&dev_name);
        if (ret)
        {
            proc_seq_printf(seq,
                            "%-18s"
                            "0x%-11x"
                            "0x%-11x"
                            "%-24s"
                            "%-10s"
                            "%s\n",
                            ip_str, 0x1, state, hw_str, "*", dev_name);
        }
    }

    return 0;
}

static ssize_t proc_arp_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                             off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        update_arp_file(&data->seq);
    }
    /* 使用proc_seq_read读取数据 */
    return proc_seq_read(&data->seq, buffer, count);
}

static int proc_pcap_open(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = calloc(1, sizeof(struct seq_data));
    off_t ppos = 0;

    if (!data)
    {
        return -ENOMEM;
    }
    *(struct seq_data **)buffer = data;

    proc_seq_init(&data->seq, data->buf, sizeof(data->buf), &ppos);
    proc_seq_putc(&data->seq, (const char)(pcap_enable_flag + '0'));

    return 0;
}

static ssize_t proc_pcap_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count, off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;

    return proc_seq_read(&data->seq, buffer, count);
}

static ssize_t proc_pcap_write(struct proc_dir_entry *entry, void *priv, const char *buffer, size_t count, off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;

    switch(buffer[0])
    {
        case '0':
            pcap_enable_flag = 0;
        break;

        case '1':
            pcap_enable_flag = 1;
        break;

        default:
            printk("Write to /proc/net/pcap/ Error Value!\n");
        return 0;
    }

    proc_seq_putc(&data->seq, buffer[0]);

    return 1;
}