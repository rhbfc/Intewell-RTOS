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
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/procfs.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <ttosProcess.h>
#include <ttos_init.h>

#define KLOG_TAG "PROCFS"
#include <klog.h>

struct seq_data
{
    char buf[8192];
    struct proc_seq_buf seq;
};
static int proc_file_open(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = calloc(1, sizeof(struct seq_data));
    if (!data)
    {
        return -ENOMEM;
    }
    *(struct seq_data **)buffer = data;
    return 0;
}
static int proc_file_release(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = buffer;
    if (data)
    {
        free(data);
    }
    return 0;
}

static void foreach_region(struct mm *mm, struct mm_region *region, void *p)
{
    struct stat stbuf;
    int ret;
    struct proc_seq_buf *seq = p;
    bool is_file = !!(region->flags & MAP_IS_FILE);

    if (is_file)
    {
        ret = stat(region->filepath, &stbuf);
        if (ret < 0)
        {
            is_file = false;
        }
    }
    else
    {
        stbuf.st_ino = 0;
    }

    proc_seq_printf(
        seq, "%16lx-%-16lx %s%s%s%s %08" PRIxPTR " %3d:%2d %3" PRId64 " \t\t%s\n",
        region->virtual_address,
        region->virtual_address + region->region_page_count * ttosGetPageSize(),
        (region->mem_attr & MT_NO_ACCESS) ? "-" : "r", (region->mem_attr & MT_RW) ? "w" : "-",
        (region->mem_attr & MT_EXECUTE_NEVER) ? "-" : "x", (region->flags & MAP_SHARED) ? "s" : "p",
        region->offset, is_file ? major(stbuf.st_dev) : 0, is_file ? minor(stbuf.st_dev) : 0,
        stbuf.st_ino,
        is_file
            ? region->filepath
            : ((region->flags & MAP_IS_STACK) ? "[stack]"
                                              : ((region->flags & MAP_IS_HEAP) ? "[heap]" : "")));
}

static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;

    TASK_ID task;
    pcb_t pcb = pcb_get_by_pid_nt(inode->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(inode->pid);
        if (task == NULL)
        {
            return -ENOENT;
        }
        pcb = task->ppcb;
    }

    if (pcb == NULL)
    {
        return 0;
    }

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);
        if (pcb_get(pcb))
        {
            mm_foreach_region(get_process_mm(pcb), foreach_region, &data->seq);
            pcb_put(pcb);
        }
    }

    return proc_seq_read(&data->seq, buffer, count);
}

static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};

int create_pid_maps(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_create("maps", 0444, parent, &ops);
    entry->pid = pid;
    return 0;
}
