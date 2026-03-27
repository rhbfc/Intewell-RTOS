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

/* /proc/[pid]/environ */

#include <errno.h>
#include <fs/procfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ttosProcess.h>
#include <ttos_init.h>

struct seq_data
{
    char buf[4096];
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
    const char *const *kenvp;
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

        kenvp = PROCESS_OBJ_GET(pcb->envp, const char *const *);

        while (*kenvp)
        {
            proc_seq_printf(&data->seq, "%s", *kenvp++);
            proc_seq_putc(&data->seq, '\0');
        }
    }

    return proc_seq_read(&data->seq, buffer, count);
}

static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};

int create_pid_environ(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_create("environ", 0444, parent, &ops);
    entry->pid = pid;
    return 0;
}
