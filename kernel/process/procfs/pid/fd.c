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
#include <stdio.h>
#include <ttosProcess.h>

struct procfs_fd_dir_iter
{
    int *fd_list;
    int index;
    int fd_list_size;
};

static int fd_get_path(pcb_t pcb, int fd, char *buf)
{
    int ret;
    struct file *filep;
    ret = fs_getfilep_by_list(fd, &filep, pcb_get_files(pcb));
    if (ret < 0)
    {
        return ret;
    }
    ret = file_ioctl(filep, FIOC_FILEPATH, buf);
    return ret;
}

static void file_callback(int fd, void *ctx)
{
    struct procfs_fd_dir_iter *iter = (struct procfs_fd_dir_iter *)ctx;
    iter->fd_list_size++;
    iter->fd_list = realloc(iter->fd_list, iter->fd_list_size * sizeof(int));
    iter->fd_list[iter->fd_list_size - 1] = fd;
}

static int proc_fd_readdir(struct proc_dir_entry *dir, struct proc_dir_entry *entry,
                           void **iter_data)
{
    char name[16];
    char buf[PATH_MAX];
    struct procfs_fd_dir_iter *iter;
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

    /* 如果entry为NULL，重置迭代状态 */
    if (!entry)
    {
        if (*iter_data)
        {
            iter = *(struct procfs_fd_dir_iter **)iter_data;
            if (iter->fd_list)
            {
                free(iter->fd_list);
            }
            free(*iter_data);
            *iter_data = NULL;
        }
        return 0;
    }
    iter = *(struct procfs_fd_dir_iter **)iter_data;
    if (iter == NULL)
    {
        iter = (struct procfs_fd_dir_iter *)calloc(1, sizeof(struct procfs_fd_dir_iter));
        if (iter == NULL)
        {
            return -ENOMEM;
        }
        *iter_data = iter;
        files_foreach(pcb_get_files(pcb), file_callback, iter);
    }
    if (iter->index >= iter->fd_list_size)
    {
        return -ENOENT;
    }
    snprintf(name, sizeof(name), "%d", iter->fd_list[iter->index]);
    if (dynamic_entry_fill(entry, name, S_IFLNK | 0777) != 0)
    {
        return -ENOMEM;
    }

    /* 设置符号链接目标 */
    fd_get_path(pcb, iter->fd_list[iter->index], buf);
    entry->link = strdup(buf);
    if (!entry->link)
    {
        free(entry->name);
        return -ENOMEM;
    }

    iter->index++;
    return 0;
}

static int release_callback(struct proc_dir_entry *entry, void *iter_data)
{
    struct procfs_fd_dir_iter *iter = (struct procfs_fd_dir_iter *)iter_data;
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
    char buf[PATH_MAX];
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

    if (dynamic_entry_fill(entry, token, S_IFLNK | 0777) != 0)
    {
        return -ENOMEM;
    }

    /* 设置符号链接目标 */
    fd_get_path(pcb, fd, buf);
    entry->link = strdup(buf);
    if (!entry->link)
    {
        free(entry->name);
        return -ENOMEM;
    }

    return 0;
}

int create_pid_fd(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry =
        proc_dynamic_dir("fd", parent, find_sub_callback, proc_fd_readdir, release_callback, NULL);
    entry->pid = pid;
    return 0;
}
