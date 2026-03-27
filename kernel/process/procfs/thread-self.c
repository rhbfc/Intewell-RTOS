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
#include <ttos_init.h>

static const char *get_link(struct proc_dir_entry *entry, char *buffer)
{
    (void)entry;
    pcb_t pcb;
    pcb = ttosProcessSelf();
    /* 直接返回当前进程的pid */
    snprintf(buffer, PATH_MAX, "%d/task/%d", get_process_pid(pcb), pcb->taskControlId->tid);
    return buffer;
}

static int procfs_process_thread_self_init(void)
{
    proc_dynamic_link("thread-self", NULL, get_link);
    return 0;
}
INIT_EXPORT_SERVE_FS(procfs_process_thread_self_init, "register /proc/thread-self");
