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

#ifndef _PERIOD_SCHED_GROUP_H
#define _PERIOD_SCHED_GROUP_H

#include <list.h>
#include <spinlock.h>

typedef struct T_TTOS_ProcessControlBlock *pcb_t;

#define BUSY 1
#define FREE 0

#define CONFIG_TASK_GROUP_NUM 32
#define CONFIG_GROUP_NAME_LEN 32

#define GROUP_COMPLETE_TIME 1
#define GROUP_COMPLETE_EXE_TIME 2

//#define TASK_GROUP_TEST

typedef struct period_sched_group
{
    ttos_spinlock_t  lock;
    unsigned int     period_time;
    unsigned int     remain_time;
    char             name[CONFIG_GROUP_NAME_LEN];
    unsigned int     task_complete_count;
	unsigned int     group_complete_time;
	unsigned int     group_complete_exe_time;
	unsigned int     group_complete_count;
    struct list_head task_list;
    int              group_id;
    bool             status;
    unsigned int     task_num;
	bool             is_start;
} period_sched_group_t;

typedef struct pthread_period_expire_info
{
	/* 超时任务的id */
	pid_t tid;

	/* 超时所在的周期(即在第几个周期超时的) */
	unsigned int timeout_period;

	/* 超时周期的执行时间(us) */
	unsigned long long exe_time;

	/* 超时周期的完成时间(us) */	
	unsigned long long complete_time;
}pthread_period_expire_info_t;

void wait_actived_task_add (pid_t tid);
void wait_actived_task_del (pid_t tid);
int  period_thread_active_all (void);
int period_thread_active_group (unsigned int period);
int  period_sched_group_get_task_num (int sched_group_id);
int  period_sched_group_work_complete (void);
void period_sched_group_notify (T_UWORD ticks);
int  period_sched_group_create (T_UWORD period, const char *name);
int  period_sched_group_add (pcb_t pcb, int sched_group_id);
int  period_sched_group_del (int sched_group_id);
int  period_thread_get_unstart_num (void);

int syscall_period_thread_active (int period);
int syscall_period_sched_group_add (pid_t tid, int sched_group_id);
int syscall_period_sched_group_create (T_UWORD period, const char *name);
int syscall_period_thread_get_unstart_num (void);
int syscall_pthread_getperiodcount (pid_t tid, int periodtime);
int syscall_pthread_getperiodexecutetime (pid_t tid, int periodtime);
int syscall_pthread_getperiodcompletetime(pid_t tid, int period);
int syscall_pthread_getperiodexpireinfo (pthread_period_expire_info_t *pthread_period_expire_info);
int sched_group_complete_count_get (int period);
int sched_group_exe_time_get (int period);
bool period_sched_group_set_start (int period);
bool period_sched_group_is_start (int period);
void ttos_set_task_timeout_info (unsigned long long exe_time, unsigned long long complete_time);
void period_sched_group_exit (pcb_t pcb);
int period_sched_group_remove (pcb_t pcb, int sched_group_id);
void period_thread_active_single (pcb_t pcb);
#endif