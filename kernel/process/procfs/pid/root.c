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

#include <fs/procfs.h>
#include <stdio.h>
#include <ttosProcess.h>

static const char *get_link(struct proc_dir_entry *entry, char *buffer)
{
    TASK_ID task;
    pcb_t pcb = pcb_get_by_pid_nt(entry->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(entry->pid);
        if (task == NULL)
        {
            return NULL;
        }
        pcb = task->ppcb;
    }

    if (pcb == NULL)
    {
        // kernel task no aux
        strlcpy(buffer, "/", PATH_MAX);
        return buffer;
    }
    strlcpy(buffer, pcb->root, PATH_MAX);
    return buffer;
}

int create_pid_root(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_dynamic_link("root", parent, get_link);
    entry->pid = pid;
    return 0;
}
