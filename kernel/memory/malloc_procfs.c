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

#include <ttos_init.h>

#ifdef CONFIG_FS_PROCFS
#include <assert.h>
#include <errno.h>
#include <fs/procfs.h>
#include <inttypes.h>
#include <malloc.h>
#include <page.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ttosMM.h>
#ifdef CONFIG_SLUB
#include <slub.h>
#endif

#define KLOG_TAG "MALLOC"
#include <klog.h>

#define MEMINFO_LINELEN 30

struct seq_data
{
    char buf[4096];
    struct proc_seq_buf seq;
};

static int meminfo_open(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = calloc(1, sizeof(struct seq_data));
    if (!data)
    {
        return -ENOMEM;
    }
    *(struct seq_data **)buffer = data;
    return 0;
}

static ssize_t meminfo_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                            off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    uintptr_t page_nr, page_free;
    struct memory_info *info = NULL;

    /* 如果是第一次读取，生成内容 */
    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);
#ifdef CONFIG_SLUB
        int total_caches, total_pages, free_pages;
        slub_get_info(&total_caches, &total_pages, &free_pages);
        page_nr = total_pages;
        page_free = free_pages;

        proc_seq_printf(&data->seq, "MemTotal: %14" PRId64 " kB\n",
                        (uint64_t)(page_nr * PAGE_SIZE) / 1024);
        proc_seq_printf(&data->seq, "MemFree: %15" PRId64 " kB\n",
                        (uint64_t)((page_free * PAGE_SIZE)) / 1024);
        proc_seq_printf(&data->seq, "MemAvailable: %10" PRId64 " kB\n",
                        (uint64_t)((page_free * PAGE_SIZE)) / 1024);
#else
        page_get_info(&page_nr, &page_free);
        info = get_malloc_info();

        proc_seq_printf(&data->seq, "MemTotal: %14" PRId64 " kB\n",
                        (uint64_t)(page_nr * PAGE_SIZE) / 1024);
        proc_seq_printf(&data->seq, "MemFree: %15" PRId64 " kB\n",
                        (uint64_t)(info->free + (page_free * PAGE_SIZE)) / 1024);
        proc_seq_printf(&data->seq, "MemAvailable: %10" PRId64 " kB\n",
                        (uint64_t)(info->free + (page_free * PAGE_SIZE)) / 1024);

#endif

        /* 使用proc_seq_printf生成内容 */
        proc_seq_printf(&data->seq, "Buffers: %15d kB\n", 0);
        proc_seq_printf(&data->seq, "Cached: %16d kB\n", 0);
        proc_seq_printf(&data->seq, "SwapCached: %12d kB\n", 0);
        proc_seq_printf(&data->seq, "Active: %16d kB\n", 0);
        proc_seq_printf(&data->seq, "Inactive: %14d kB\n", 0);
        proc_seq_printf(&data->seq, "Active(anon): %10d kB\n", 0);
        proc_seq_printf(&data->seq, "Inactive(anon): %8d kB\n", 0);
        proc_seq_printf(&data->seq, "Unevictable: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "Mlocked: %15d kB\n", 0);
        proc_seq_printf(&data->seq, "SwapTotal: %13d kB\n", 0);
        proc_seq_printf(&data->seq, "SwapFree: %14d kB\n", 0);
        proc_seq_printf(&data->seq, "Dirty: %17d kB\n", 0);
        proc_seq_printf(&data->seq, "Writeback: %13d kB\n", 0);
        proc_seq_printf(&data->seq, "AnonPages: %13d kB\n", 0);
        proc_seq_printf(&data->seq, "Mapped: %16d kB\n", 0);
        proc_seq_printf(&data->seq, "Shmem: %17d kB\n", 0);
        proc_seq_printf(&data->seq, "KReclaimable: %10d kB\n", 0);

#ifdef CONFIG_SLUB
        proc_seq_printf(&data->seq, "Slab: %18d kB\n",
                        (uint64_t)((total_caches * PAGE_SIZE)) / 1024);
        proc_seq_printf(&data->seq, "SReclaimable: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "SUnreclaim: %12d kB\n",
                        (uint64_t)((total_caches * PAGE_SIZE)) / 1024);
#else
        proc_seq_printf(&data->seq, "Slab: %18d kB\n", 0);
        proc_seq_printf(&data->seq, "SReclaimable: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "SUnreclaim: %12d kB\n", 0);
#endif
        proc_seq_printf(&data->seq, "KernelStack: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "PageTables: %12d kB\n", 0);
        proc_seq_printf(&data->seq, "NFS_Unstable: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "Bounce: %16d kB\n", 0);
        proc_seq_printf(&data->seq, "WritebackTmp: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "CommitLimit: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "Committed_AS: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "VmallocTotal: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "VmallocUsed: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "VmallocChunk: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "Percpu: %16d kB\n", 0);
        proc_seq_printf(&data->seq, "HardwareCorrupted: %5d kB\n", 0);
        proc_seq_printf(&data->seq, "AnonHugePages: %9d kB\n", 0);
        proc_seq_printf(&data->seq, "ShmemHugePages: %8d kB\n", 0);
        proc_seq_printf(&data->seq, "ShmemPmdMapped: %8d kB\n", 0);
        proc_seq_printf(&data->seq, "FileHugePages: %9d kB\n", 0);
        proc_seq_printf(&data->seq, "FilePmdMapped: %9d kB\n", 0);
        proc_seq_printf(&data->seq, "HugePages_Total: %7d\n", 0);
        proc_seq_printf(&data->seq, "HugePages_Free: %8d\n", 0);
        proc_seq_printf(&data->seq, "HugePages_Rsvd: %8d\n", 0);
        proc_seq_printf(&data->seq, "HugePages_Surp: %8d\n", 0);
        proc_seq_printf(&data->seq, "Hugepagesize: %10d kB\n", 0);
        proc_seq_printf(&data->seq, "Hugetlb: %15d kB\n", 0);
        proc_seq_printf(&data->seq, "DirectMap4k: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "DirectMap2M: %11d kB\n", 0);
        proc_seq_printf(&data->seq, "DirectMap1G: %11d kB\n", 0);
    }

    /* 使用proc_seq_read读取数据 */
    return proc_seq_read(&data->seq, buffer, count);
}

static int meminfo_release(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = buffer;
    if (data)
    {
        free(data);
    }
    return 0;
}

/* 序列化文件操作结构体 */
static struct proc_ops meminfo_ops = {
    .proc_open = meminfo_open, .proc_read = meminfo_read, .proc_release = meminfo_release};
static int procfs_meminfo_init(void)
{
    proc_create("meminfo", 0444, NULL, &meminfo_ops);
    return 0;
}
INIT_EXPORT_SERVE_FS(procfs_meminfo_init, "procfs meminfo init");
#endif