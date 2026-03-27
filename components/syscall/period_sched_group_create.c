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

#include "syscall_internal.h"
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdarg.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttosProcess.h>
#include <uaccess.h>
#include <period_sched_group.h>

int sched_group_complete_time_get (int period);

/**
 * @brief 系统调用实现：创建周期调度组。
 *
 * 该函数实现了一个系统调用，用于创建周期调度组。
 *
 * @param period 周期时间。
 * @param name 调度组名称。
 * @return 成功时返回调度组 ID，失败时返回负值错误码。
 */
int syscall_period_sched_group_create(unsigned int period, const char *name)
{
    int len = 0;
    char group_name[CONFIG_GROUP_NAME_LEN] = {0};

    len = strlen(name) + 1;
    len = (len >= CONFIG_GROUP_NAME_LEN)?CONFIG_GROUP_NAME_LEN:len;
    
    if (0 != copy_from_user (group_name, name, len))
    {
        return -EINVAL;
    }

    return period_sched_group_create (period, group_name);
}

int syscall_pthread_getperiodcount (pid_t tid, int periodtime)
{
	T_TTOS_TaskControlBlock *thread = NULL;
	
	/* periodtimeΪ0��ʾ��ȡָ��������ɴ���*/
	if (0 == periodtime)
	{
    	thread = task_get_by_tid(tid);
		if (!thread || TTOS_SCHED_PERIOD != thread->taskType)
		{
			return -1;
		}
		
		/* ��ȡָ���������ɴ��� */
		return thread->periodNode.jobCompletedCount;
	}
	else
	{
		/* ��ȡָ���������������ɴ��� */
		return sched_group_complete_count_get(periodtime);
	}
}

int syscall_pthread_getperiodexecutetime (pid_t tid, int periodtime)
{
	T_TTOS_TaskControlBlock *thread;

	/* periodtimeΪ0��ʾ��ȡָ���������һ�����������ʵ�����ĵ�ʱ�� */
	if (0 == periodtime)
	{
		thread = task_get_by_tid(tid);
		if (!thread || TTOS_SCHED_PERIOD != thread->taskType)
		{
			return -1;
		}

		return thread->periodNode.periodexecutecompletetime;
	}
	else /* periodtime��Ϊ0��ʾ��ȡָ���������һ�����������ʵ�����ĵ�ʱ�� */
	{
		return sched_group_exe_time_get(periodtime);
	}
}

int syscall_pthread_getperiodcompletetime(pid_t tid, int period)
{	
	T_TTOS_TaskControlBlock *thread;

	/* ��ȡָ�������������һ�����ڵ����ʱ�� */
	if (0 == period)
	{
		thread = task_get_by_tid(tid);
		if (TTOS_SCHED_PERIOD != thread->taskType)
		{
			return -1;
		}

		return thread->periodNode.periodcompletetime;
	}
	else /* ��ȡָ�����ڵ����������������һ�����ڵ����ʱ�� */
	{
		return sched_group_complete_time_get (period);
	}
}

static pthread_period_expire_info_t pthread_period_expire_info;
static bool period_time_out = false;

void ttos_set_task_timeout (TASK_ID task)
{
	/* ������¼��һ�γ�ʱ�������Ϣ */
	if (period_time_out)
	{
		return;
	}

	period_time_out = true;

	/* ��¼��ʱ�����tid�ͳ�ʱ���ڵ����� */
	pthread_period_expire_info.tid = task->tid;
	pthread_period_expire_info.timeout_period = task->periodNode.jobCompletedCount +1;

	task->periodNode.isTimeout = true;
}

void ttos_set_task_timeout_info (unsigned long long exe_time, unsigned long long complete_time)
{
	static bool is_timeout = false;
	/* ������¼��һ�γ�ʱ�������Ϣ */
	if (is_timeout)
	{
		return;
	}

	is_timeout = true;

	/* ��ʱ���ڵ�ִ��ʱ��(us) */
	pthread_period_expire_info.exe_time = exe_time;

	/* ��ʱ���ڵ����ʱ��(us) */	
	pthread_period_expire_info.complete_time = complete_time;
}

int syscall_pthread_getperiodexpireinfo (pthread_period_expire_info_t *info)
{
	if (!info)
	{
		return EINVAL;
	}

	/* �����ڳ�ʱ������ */
	if (!period_time_out)
	{
		return ESRCH;
	}

	if (copy_to_user(info, &pthread_period_expire_info, sizeof(*info)))
	{
		return EINVAL;
	}
	else
	{
		return 0;
	}
}
