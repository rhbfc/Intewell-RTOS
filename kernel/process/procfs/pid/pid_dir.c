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
#include <ttos_init.h>

struct procfs_task_dir_iter
{
    int *task_list;
    int index;
    int task_list_size;
};

static void task_enum(TASK_ID task, void *ctx)
{
    struct procfs_task_dir_iter *iter = (struct procfs_task_dir_iter *)ctx;
    pcb_t pcb = task->ppcb;
    if (pcb && !pcb_get(pcb))
    {
        return;
    }
    /* 只有内核线程或者用户态进程生成文件夹 */
    if (pcb == NULL || pcb->group_leader == pcb)
    {
        iter->task_list_size++;
        iter->task_list = realloc(iter->task_list, iter->task_list_size * sizeof(int));
        iter->task_list[iter->task_list_size - 1] = (task->tid);
    }
    if (pcb)
    {
        pcb_put(pcb);
    }
}

int fill_task_entry(pid_t tid, struct proc_dir_entry *entry)
{
    char name[16];
    snprintf(name, sizeof(name), "%d", tid);
    if (dynamic_entry_fill(entry, name, S_IFDIR | 0777) != 0)
    {
        return -ENOMEM;
    }

    create_pid_stat(tid, entry);
    create_pid_cmdline(tid, entry);
    create_pid_comm(tid, entry);
    create_pid_cwd(tid, entry);
    create_pid_environ(tid, entry);
    create_pid_exe(tid, entry);
    create_pid_fd(tid, entry);
    create_pid_fdinfo(tid, entry);
    create_pid_maps(tid, entry);
    create_pid_root(tid, entry);
    return 0;
}

static int fill_pid_entry(pid_t tid, struct proc_dir_entry *entry)
{
    fill_task_entry(tid, entry);
    create_pid_task(tid, entry);
    return 0;
}

static int proc_root_readdir(struct proc_dir_entry *dir, struct proc_dir_entry *entry,
                             void **iter_data)
{
    char name[16];
    char buf[32];
    struct procfs_task_dir_iter *iter;
    TASK_ID task;
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

        task_foreach(task_enum, iter);
    }
    if (iter->index >= iter->task_list_size)
    {
        return -ENOENT;
    }

    iter->index++;
    return fill_pid_entry(iter->task_list[iter->index - 1], entry);
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
    pcb_t pcb = NULL;
    TASK_ID task;

    tid = atoi(token);
    if (tid < 0)
    {
        return -ENOENT;
    }

    task = task_get_by_tid(tid);
    if (task == NULL)
    {
        return -ENOENT;
    }

    if (pcb && pcb->group_leader != pcb)
    {
        return -ENOENT;
    }

    return fill_pid_entry(tid, entry);
}

static int pid_dir_init(void)
{
    struct proc_dir_entry *entry;

    entry = find_entry_by_path("/");
    entry->find_sub_callback = find_sub_callback;
    entry->readdir_callback = proc_root_readdir;
    entry->release_callback = release_callback;
    entry->readdir_data = NULL;

    return 0;
}
INIT_EXPORT_SERVE_FS(pid_dir_init, "procfs init pid dir");