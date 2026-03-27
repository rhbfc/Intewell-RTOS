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

#include "pid_dir.h"
#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/procfs.h>
#include <stdio.h>
#include <ttosProcess.h>

struct procfs_task_dir_iter
{
    int *task_list;
    int index;
    int task_list_size;
};

static void task_enum(pcb_t pcb, void *ctx)
{
    struct procfs_task_dir_iter *iter = (struct procfs_task_dir_iter *)ctx;
    iter->task_list_size++;
    iter->task_list = realloc(iter->task_list, iter->task_list_size * sizeof(int));
    iter->task_list[iter->task_list_size - 1] = (pcb->taskControlId->tid);
}

static int proc_pid_task_readdir(struct proc_dir_entry *dir, struct proc_dir_entry *entry,
                                 void **iter_data)
{
    char name[16];
    char buf[32];
    struct procfs_task_dir_iter *iter;
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
            iter = *(struct procfs_task_dir_iter **)iter_data;
            if (iter->task_list)
            {
                free(iter->task_list);
            }
            free(*iter_data);
            *iter_data = NULL;
        }
        return 0;
    }
    iter = *(struct procfs_task_dir_iter **)iter_data;
    if (iter == NULL)
    {
        iter = (struct procfs_task_dir_iter *)calloc(1, sizeof(struct procfs_task_dir_iter));
        if (iter == NULL)
        {
            return -ENOMEM;
        }
        *iter_data = iter;

        if (pcb == NULL)
        {
            iter->task_list_size = 1;
            iter->task_list = malloc(sizeof(int));
            iter->task_list[0] = dir->pid;
        }
        else
        {
            foreach_task_group(pcb->group_leader, task_enum, iter);
        }
    }
    if (iter->index >= iter->task_list_size)
    {
        return -ENOENT;
    }

    iter->index++;
    return fill_task_entry(iter->task_list[iter->index - 1], entry);
}

static int release_callback(struct proc_dir_entry *entry, void *iter_data)
{
    struct procfs_task_dir_iter *iter = (struct procfs_task_dir_iter *)iter_data;
    if (iter)
    {
        if (iter->task_list)
        {
            free(iter->task_list);
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
    char buf[32];
    int tid;
    pcb_t child = NULL;
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

    tid = atoi(token);
    if (tid < 0)
    {
        return -ENOENT;
    }

    if (pcb == NULL)
    {
        // kernel task;
        if (tid != dir->pid)
        {
            return -ENOENT;
        }
    }
    else
    {
        task = task_get_by_tid(tid);
        if (task == NULL || task->ppcb == NULL)
        {
            return -ENOENT;
        }
        child = task->ppcb;
        if (child->group_leader != pcb->group_leader)
        {
            return -ENOENT;
        }
    }

    return fill_task_entry(tid, entry);
}

int create_pid_task(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_dynamic_dir("task", parent, find_sub_callback, proc_pid_task_readdir,
                             release_callback, NULL);
    entry->pid = pid;
    return 0;
}
