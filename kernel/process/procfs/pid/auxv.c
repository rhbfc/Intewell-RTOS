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

/* /proc/[pid]/auxv */

#include "uaccess.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/procfs.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ttosProcess.h>
#include <ttos_init.h>

#define KLOG_TAG "PROCFS"
#include <klog.h>

static int proc_file_open(struct proc_dir_entry *inode, void *buffer)
{
    (void)inode;
    (void)buffer;
    return 0;
}

static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    size_t copy_size = 0;
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
        // kernel task no aux
        return 0;
    }

    if (pcb_get(pcb))
    {
        if (*ppos >= pcb->aux_len)
        {
            pcb_put(pcb);
            return 0;
        }
        copy_size = pcb->aux_len - *ppos;
        if (copy_size > count)
        {
            copy_size = count;
        }

        phys_addr_t pa = ALIGN_DOWN(pcb->auxvp, PAGE_SIZE);
        phys_addr_t off = pcb->auxvp - pa;
        virt_addr_t va = ktmp_access_start(pa);
        memcpy(buffer, (void *)(virt_addr_t)(va + off + *ppos), copy_size);

        *ppos += copy_size;
        ktmp_access_end(va);
        pcb_put(pcb);
    }

    return copy_size;
}

static int proc_file_release(struct proc_dir_entry *inode, void *buffer)
{
    (void)inode;
    (void)buffer;
    return 0;
}
static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};

int create_pid_auxv(pid_t pid)
{
    char path[128];
    struct proc_dir_entry *entry;
    snprintf(path, sizeof(path), "%d/auxv", pid);
    entry = proc_create(path, 0444, NULL, &ops);
    entry->pid = pid;
    return 0;
}
