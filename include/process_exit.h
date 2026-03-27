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

#ifndef PROCESS_EXIT_H
#define PROCESS_EXIT_H

#include <system/types.h>
#include <signal.h>
#include <ttosProcess.h>
#include <sys/wait.h>

typedef struct
{
    int   exit_status;
    pid_t pid;

} wait_stat;

typedef struct
{
    pid_t                 pid;
    int                   options;
    int __user           *stat_addr;
    struct rusage __user *ru;
    wait_stat            stat;
} wait_handle_t;

typedef enum
{
    STOPPED,
    EXITED,
} process_exit_state;

pcb_t wait_process_zombie(pcb_t pcb, wait_handle_t *wait_handle);
pcb_t wait_thread_zombie(pcb_t pcb, wait_handle_t *wait_handle);
int wait_process(pcb_t target, wait_handle_t *wait_handle);
#endif /* PROCESS_EXIT_H */
