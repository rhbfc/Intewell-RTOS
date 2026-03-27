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

#ifndef __PID_DIR_H__
#define __PID_DIR_H__
#include <sys/types.h>

struct proc_dir_entry;

int create_pid_root(pid_t pid, struct proc_dir_entry *parent);
int create_pid_cwd(pid_t pid, struct proc_dir_entry *parent);
int create_pid_exe(pid_t pid, struct proc_dir_entry *parent);
int create_pid_auxv(pid_t pid, struct proc_dir_entry *parent);
int create_pid_cmdline(pid_t pid, struct proc_dir_entry *parent);
int create_pid_comm(pid_t pid, struct proc_dir_entry *parent);
int create_pid_maps(pid_t pid, struct proc_dir_entry *parent);
int create_pid_fd(pid_t pid, struct proc_dir_entry *parent);
int create_pid_fdinfo(pid_t pid, struct proc_dir_entry *parent);
int create_pid_environ(pid_t pid, struct proc_dir_entry *parent);
int create_pid_task(pid_t pid, struct proc_dir_entry *parent);
int create_pid_stat(pid_t pid, struct proc_dir_entry *parent);

int fill_task_entry(pid_t tid, struct proc_dir_entry *entry);

#endif /* __PID_DIR_H__ */
