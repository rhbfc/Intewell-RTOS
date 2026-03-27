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

/* /proc/[pid]/comm */

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

struct seq_data
{
    char buf[NAME_MAX];
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

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);
        if (pcb == NULL)
        {
            proc_seq_printf(&data->seq, "%s\n", (char *)task->objCore.objName);
        }
        else
        {
            if (!pcb_get(pcb))
            {
                return -ENOENT;
            }
            proc_seq_printf(&data->seq, "%s\n", (char *)pcb->cmd_name);
            pcb_put(pcb);
        }
    }

    return proc_seq_read(&data->seq, buffer, count);
}

static ssize_t proc_file_write(struct proc_dir_entry *inode, void *priv, const char *buffer,
                               size_t count, off_t *ppos)
{
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
        if (count > sizeof(task->objCore.objName))
        {
            count = sizeof(task->objCore.objName) - 1;
        }
        memcpy(task->objCore.objName, buffer, count);
        task->objCore.objName[count] = '\0';
        return -ENOENT;
    }
    else
    {
        if (!pcb_get(pcb))
        {
            return -ENOENT;
        }
        if (count > sizeof(pcb->cmd_name))
        {
            count = sizeof(pcb->cmd_name) - 1;
            pcb->cmd_name[count] = '\0';
        }
        memcpy(pcb->cmd_name, buffer, count);
        pcb->cmd_name[count] = '\0';
        pcb_put(pcb);
    }
    return count;
}
static struct proc_ops ops = {.proc_open = proc_file_open,
                              .proc_read = proc_file_read,
                              .proc_write = proc_file_write,
                              .proc_release = proc_file_release};

int create_pid_comm(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_create("comm", 0666, parent, &ops);
    entry->pid = pid;
    return 0;
}
