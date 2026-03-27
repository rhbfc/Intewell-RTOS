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

#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/procfs.h>
#include <inttypes.h>
#include <stdio.h>
#include <ttosProcess.h>

struct procfs_fdinfo_dir_iter
{
    int *fd_list;
    int index;
    int fd_list_size;
};

struct seq_data
{
    char buf[512];
    struct proc_seq_buf seq;
};

static struct proc_ops ops;

static void file_callback(int fd, void *ctx)
{
    struct procfs_fdinfo_dir_iter *iter = (struct procfs_fdinfo_dir_iter *)ctx;
    iter->fd_list_size++;
    iter->fd_list = realloc(iter->fd_list, iter->fd_list_size * sizeof(int));
    iter->fd_list[iter->fd_list_size - 1] = fd;
}

static int proc_fdinfo_readdir(struct proc_dir_entry *dir, struct proc_dir_entry *entry,
                               void **iter_data)
{
    char name[16];
    struct procfs_fdinfo_dir_iter *iter;
    TASK_ID task;
    pcb_t pcb = pcb_get_by_pid_nt(dir->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(dir->pid);
        if (task == NULL)
        {
            return -ENOENT;
        }
        pcb = task->ppcb;
    }

    if (!entry)
    {
        if (*iter_data)
        {
            iter = *(struct procfs_fdinfo_dir_iter **)iter_data;
            if (iter->fd_list)
            {
                free(iter->fd_list);
            }
            free(*iter_data);
            *iter_data = NULL;
        }
        return 0;
    }

    iter = *(struct procfs_fdinfo_dir_iter **)iter_data;
    if (iter == NULL)
    {
        iter = (struct procfs_fdinfo_dir_iter *)calloc(1, sizeof(struct procfs_fdinfo_dir_iter));
        if (iter == NULL)
        {
            return -ENOMEM;
        }
        *iter_data = iter;
        if (pcb && pcb_get(pcb))
        {
            files_foreach(pcb_get_files(pcb), file_callback, iter);
            pcb_put(pcb);
        }
        else if (pcb == NULL)
        {
            files_foreach(pcb_get_files(NULL), file_callback, iter);
        }
    }

    if (iter->index >= iter->fd_list_size)
    {
        return -ENOENT;
    }

    snprintf(name, sizeof(name), "%d", iter->fd_list[iter->index]);
    if (dynamic_entry_fill(entry, name, S_IFREG | 0444) != 0)
    {
        return -ENOMEM;
    }

    entry->proc_ops = &ops;
    entry->pid = dir->pid;
    iter->index++;
    return 0;
}

static int release_callback(struct proc_dir_entry *entry, void *iter_data)
{
    struct procfs_fdinfo_dir_iter *iter = (struct procfs_fdinfo_dir_iter *)iter_data;
    if (iter)
    {
        if (iter->fd_list)
        {
            free(iter->fd_list);
        }
        free(iter);
    }
    return 0;
}

static int find_sub_callback(struct proc_dir_entry *dir, struct proc_dir_entry *entry,
                             const char *token)
{
    struct file *filep;
    int ret;
    TASK_ID task;
    int fd;
    pcb_t pcb = pcb_get_by_pid_nt(dir->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(dir->pid);
        if (task == NULL)
        {
            return -ENOENT;
        }
        pcb = task->ppcb;
    }

    fd = atoi(token);
    if (fd < 0)
    {
        return -ENOENT;
    }

    ret = fs_getfilep_by_list(fd, &filep, pcb_get_files(pcb));
    if (filep == NULL || ret < 0)
    {
        return -ENOENT;
    }

    if (dynamic_entry_fill(entry, token, S_IFREG | 0444) != 0)
    {
        return -ENOMEM;
    }

    entry->proc_ops = &ops;
    entry->pid = dir->pid;
    return 0;
}

static int proc_fdinfo_open(struct proc_dir_entry *entry, void *priv)
{
    struct seq_data *data;
    TASK_ID task;
    // task
    task = task_get_by_tid(entry->pid);
    if (task == NULL)
    {
        return -ENOENT;
    }

    data = calloc(1, sizeof(struct seq_data));
    if (!data)
    {
        return -ENOMEM;
    }

    *(struct seq_data **)priv = data;
    return 0;
}

static ssize_t proc_fdinfo_read(struct proc_dir_entry *entry, void *priv, char *buffer,
                                size_t count, off_t *ppos)
{
    struct seq_data *data = priv;
    TASK_ID task;
    pcb_t pcb = pcb_get_by_pid_nt(entry->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(entry->pid);
        if (task == NULL)
        {
            return -ENOENT;
        }
        pcb = task->ppcb;
    }

    /* 如果是第一次读取，生成内容 */
    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        struct file *filep = process_getfile(pcb, atoi(entry->name));

        /* 使用proc_seq_printf生成内容 */
        proc_seq_printf(&data->seq, "pos:    %" PRId64 "\n", filep->f_pos);
        proc_seq_printf(&data->seq, "flags:  %07o\n", filep->f_oflags);
        proc_seq_printf(&data->seq, "mnt_id: 0\n");
        proc_seq_printf(&data->seq, "ino:    %" PRIu64 "\n", filep->f_inode->i_ino);
    }

    /* 使用proc_seq_read读取数据 */
    return proc_seq_read(&data->seq, buffer, count);
}

static int proc_fdinfo_release(struct proc_dir_entry *entry, void *priv)
{
    struct seq_data *data = priv;
    if (data)
    {
        free(data);
    }
    return 0;
}

static struct proc_ops ops = {.proc_open = proc_fdinfo_open,
                              .proc_read = proc_fdinfo_read,
                              .proc_release = proc_fdinfo_release};

int create_pid_fdinfo(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_dynamic_dir("fdinfo", parent, find_sub_callback, proc_fdinfo_readdir,
                             release_callback, NULL);
    entry->pid = pid;
    return 0;
}
