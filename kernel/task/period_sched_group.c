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

#include <ttos.h>
#include <ttosInterHal.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <period_sched_group.h>

#undef KLOG_TAG
#define KLOG_TAG "ttosTaskGroup"
#include <klog.h>

#define PERIOD_ALL -1

void ttosDisableTaskDispatchWithLock (void);
void ttosEnableTaskDispatchWithLock (void);

T_MODULE ttos_spinlock_t sched_group_lock;
T_MODULE period_sched_group_t sched_group_array[CONFIG_TASK_GROUP_NUM];
T_MODULE struct list_head wait_actived_task_list;

static period_sched_group_t *cur_priod_group;

void wait_actived_task_add (pid_t tid)
{
    pcb_t pcb;
    T_TTOS_TaskControlBlock *thread;
    unsigned long flags = 0;

    thread = task_get_by_tid(tid);
    pcb = thread->ppcb;

    spin_lock_irqsave(&sched_group_lock, flags);
    list_add(&pcb->non_auto_start_node ,&wait_actived_task_list);
    spin_unlock_irqrestore (&sched_group_lock, flags);
}

void wait_actived_task_del (pid_t tid)
{
    pcb_t pcb;
    T_TTOS_TaskControlBlock *thread;

    thread = task_get_by_tid(tid);
    pcb = thread->ppcb;

    list_del(&pcb->non_auto_start_node);
}

/* 根据参数启动所有未启动的周期任务或指定周期的周期任务 */
int period_thread_active (int period)
{
    pcb_t node;
    pcb_t next;
    int cnt = 0;
    unsigned long flags = 0;

    //关调度
    ttosDisableTaskDispatchWithLock ();

    spin_lock_irqsave(&sched_group_lock, flags);

    list_for_each_entry_safe(node, next, &wait_actived_task_list, non_auto_start_node)
    {
    	if (PERIOD_ALL == period || period == node->taskControlId->periodNode.periodTime)
    	{
            cnt++;
			list_delete(&node->non_auto_start_node);
			TTOS_ActiveTask(node->taskControlId);
    	}
		
    }

	if (PERIOD_ALL == period)
	{
    	INIT_LIST_HEAD(&wait_actived_task_list);
	}

    spin_unlock_irqrestore (&sched_group_lock, flags);
    //开调度
    ttosEnableTaskDispatchWithLock ();

    return cnt;
}

/* 启动所有未启动的周期任务 */
int period_thread_active_all (void)
{
	return period_thread_active(PERIOD_ALL);
}

/* 启动指定周期未启动的所有任务 */
int period_thread_active_group (unsigned int period)
{
	return period_thread_active(period);
}

/* 启动指定的周期任务 */
void period_thread_active_single (pcb_t pcb)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&sched_group_lock, flags);
    list_delete(&pcb->non_auto_start_node);
    spin_unlock_irqrestore (&sched_group_lock, flags);

    TTOS_ActiveTask(pcb->taskControlId);
}

static bool isValid (int sched_group_id)
{
    if (sched_group_id >= CONFIG_TASK_GROUP_NUM 
        || sched_group_id < 0 
        || FREE == sched_group_array[sched_group_id].status)
    {
        return false;
    }
    else
    {
        return true;
    }
}

T_MODULE period_sched_group_t* period_sched_group_alloc (T_VOID)
{
    unsigned long flags = 0;
    T_WORD i = 0;

    spin_lock_irqsave(&sched_group_lock, flags);

    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        if (FREE == sched_group_array[i].status)
        {
            sched_group_array[i].status = BUSY;
            spin_unlock_irqrestore (&sched_group_lock, flags);
            return &sched_group_array[i];
        }
    }

    spin_unlock_irqrestore (&sched_group_lock, flags);

    return NULL;
}


//调用该接口时，外层已经获取锁
static int period_sched_group_find (const char *name)
{
    int group_id = 0;
    T_WORD i = 0;

    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        if (BUSY == sched_group_array[i].status && !strcmp(sched_group_array[i].name, name))
        {
            group_id = sched_group_array[i].group_id;
            return group_id;
        }
    }

    return -1;
}

period_sched_group_t *get_sched_group_by_period (int period)
{
    T_WORD i = 0;

	/* 根据周期查找组 */
    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        if (BUSY == sched_group_array[i].status && sched_group_array[i].period_time == period)
        {
			return &sched_group_array[i];
        }
    }

	return NULL;
}

bool period_sched_group_is_start (int period)
{
    period_sched_group_t *group = get_sched_group_by_period (period);
    if (group)
    {
        return group->is_start;
    }
    else
    {
        return false;
    }
}

bool period_sched_group_set_start (int period)
{
    period_sched_group_t *group = get_sched_group_by_period (period);
    if (group)
    {
        cur_priod_group = group;
        return group->is_start = true;
    }
    return false;
}

period_sched_group_t *get_cur_sched_group (void)
{
    return cur_priod_group;
}

void set_cur_sched_group (period_sched_group_t *group)
{
    cur_priod_group = group;
}

void period_sched_group_exit (pcb_t pcb)
{
    int period = 0;

    if (!pcb || TTOS_SCHED_PERIOD != pcb->taskControlId->taskType)
    {
        return;
    }

    period = pcb->taskControlId->periodNode.periodTime;

    TTOS_KERNEL_LOCK ();

    period_sched_group_t *group = get_sched_group_by_period (period);
    if (!group)
    {
        TTOS_KERNEL_UNLOCK ();
        return;
    }

    period_sched_group_remove (pcb, group->group_id);

    list_delete(&pcb->taskControlId->periodNode.objectNode);

    TTOS_KERNEL_UNLOCK ();
}

int sched_group_complete_time_get (int period)
{
	period_sched_group_t *group = get_sched_group_by_period (period);
	if (group)
	{
		return group->group_complete_time;
	}
	else
	{
		return -1;
	}
}

int sched_group_complete_count_get (int period)
{
	period_sched_group_t *group = get_sched_group_by_period (period);
	if (group)
	{
		return group->group_complete_count;
	}
	else
	{
		return -1;
	}
}

int sched_group_exe_time_get (int period)
{
	period_sched_group_t *group = get_sched_group_by_period (period);
	if (group)
	{
		return group->group_complete_exe_time;
	}
	else
	{
		return -1;
	}
}

int sched_group_time_cal (int period, int type)
{
	int total_time = 0;
	pcb_t pcb = NULL;
    T_WORD i = 0;
	period_sched_group_t *group = NULL;

	/* 根据周期查找组 */
    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        if (BUSY == sched_group_array[i].status && sched_group_array[i].period_time == period)
        {
			group = &sched_group_array[i];
        }
    }

	if (!group)
	{
		return -1;
	}

	list_for_each_entry(pcb, &group->task_list, sched_group_node)
	{
        if (GROUP_COMPLETE_TIME == type)
        {
            total_time += pcb->taskControlId->periodNode.periodcompletetime;
        }
        else if (GROUP_COMPLETE_EXE_TIME  == type)
        {
             total_time += pcb->taskControlId->periodNode.periodexecutecompletetime;
        }
		
	}

	return total_time;
}

int sched_group_complete_time (int period)
{
	int time_us = 0;
    T_WORD i = 0;
	period_sched_group_t *group = NULL;

	/* 根据周期查找组 */
    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        if (BUSY == sched_group_array[i].status && sched_group_array[i].period_time == period)
        {
			group = &sched_group_array[i];
			break;
        }
    }

	if (!group)
	{
		return -1;
	}

	time_us = group->group_complete_time;

    return time_us;
}


int period_sched_group_get_task_num (int sched_group_id)
{
    if (!isValid(sched_group_id))
    {
        return -1;
    }

    return sched_group_array[sched_group_id].task_num;
}

//在调用TTOS_WaitPeriod接口中调用该接口
int period_sched_group_work_complete (void)
{
    unsigned long long end_cnt = 0;
    int sched_group_id = 0;
    unsigned long flags = 0;
    period_sched_group_t *group = NULL;
    pcb_t pcb = ttosProcessSelf();
    
    sched_group_id = pcb->sched_group_id;
    
    if (!isValid(sched_group_id))
    {
        return -1;
    }

    end_cnt = TTOS_GetCurrentSystemCount();

    group = &sched_group_array[sched_group_id];

    spin_lock_irqsave(&group->lock, flags);

    group->task_complete_count++;

    pcb->taskControlId->periodNode.jobCompletedCount++;
    pcb->taskControlId->periodNode.periodStarted = false;

    pcb->taskControlId->periodNode.periodcompletetime = 
    TTOS_GetCurrentSystemSubTime (pcb->taskControlId->periodNode.periodStartedTimestamp, end_cnt, TTOS_US_UNIT);

    /* 更新周期任务完成的实际执行时间us */
    pcb->taskControlId->periodNode.periodexecutecnt += (end_cnt-pcb->taskControlId->periodNode.exeStartedTimestamp);
	pcb->taskControlId->periodNode.periodexecutecompletetime = TTOS_GetCurrentSystemTime (pcb->taskControlId->periodNode.periodexecutecnt, TTOS_US_UNIT);
   
    pcb->taskControlId->periodNode.periodexecutecnt = 0;

    /* 该任务在超时后才完成 */
    if (pcb->taskControlId->periodNode.isTimeout)
    {
        /* 记录超时信息 */
        ttos_set_task_timeout_info (pcb->taskControlId->periodNode.periodexecutecompletetime, pcb->taskControlId->periodNode.periodcompletetime);
    }

    /* 周期组内所有任务都完成 */
    if (group->task_complete_count == group->task_num)
    {
        /* 当前调度组已经完成，将当前调度组设置为NULL */
        set_cur_sched_group(NULL);

        /* 复位周期数据 */
        group->task_complete_count = 0;
        group->remain_time = group->period_time;
        group->is_start = false;

		/* 记录组周期完成次数 */
		group->group_complete_count++;
		
		/* 累计组内所有任务的完成时间作为组本周期完成时间 */
		group->group_complete_time = sched_group_time_cal (group->period_time, GROUP_COMPLETE_TIME);

        /* 累计组内所有任务的完成执行时间作为组本周期完成执行时间 */
        group->group_complete_exe_time = sched_group_time_cal (group->period_time, GROUP_COMPLETE_EXE_TIME);
    }
    
    spin_unlock_irqrestore(&group->lock, flags);

    return 0;
}

void period_sched_group_notify (T_UWORD ticks)
{
    period_sched_group_t *cur_group = NULL;
    // T_TTOS_TaskControlBlock *task = ttosGetCurrentCpuRunningTask();

    /* 获取当前运行的调度组 */
    cur_group = get_cur_sched_group ();
    if (!cur_group || !cur_group->is_start)
    {
        return;
    }

    if (cur_group->task_complete_count == cur_group->task_num)
    {
        return;
    }

    if (cur_group->remain_time > ticks)
    {
        cur_group->remain_time -= ticks;
    }
    else
    {
        /* 任务组超时 */
        // KLOG_EMERG("period_sched_group timeout task_complete_count:%d task_num:%d period_time:%d"
        // ,cur_group->task_complete_count,cur_group->task_num, cur_group->period_time);
        //TTOS_ExpireTaskHook(task);
    }
}

int period_sched_group_create (T_UWORD period, const char *name)
{
    int id = -1;
    int len = 0;
    unsigned long flags = 0;
    period_sched_group_t *group = NULL;

    if (!name)
    {
        return -1;
    }

    spin_lock_irqsave(&sched_group_lock, flags);

    id = period_sched_group_find (name);
    if (id >= 0)
    {
        spin_unlock_irqrestore (&sched_group_lock, flags);
        /* 相同名字的组已经存在,则直接返回id */
        return id;
    }
    else
    {
        /* 相同名字的组不存在,则创建组 */

        group = period_sched_group_alloc();
        if (!group)
        {
            return -1;
        }
        
        group->period_time = period;
        group->remain_time = period;

        len = strlen(name) + 1;
        len = (len >= CONFIG_GROUP_NAME_LEN) ? CONFIG_GROUP_NAME_LEN:len;
        strncpy(group->name, name, len);
        
        INIT_LIST_HEAD(&group->task_list);
        spin_lock_init(&group->lock);

        spin_unlock_irqrestore (&sched_group_lock, flags);
        return group->group_id;
    }
}

//将一个任务加入到指定的调度组
int period_sched_group_add (pcb_t pcb, int sched_group_id)
{
    unsigned long flags = 0;

    if (!isValid(sched_group_id))
    {
        return -1;
    }

    spin_lock_irqsave(&sched_group_array[sched_group_id].lock, flags);
    list_add(&pcb->sched_group_node, &sched_group_array[sched_group_id].task_list);
    pcb->sched_group_id = sched_group_id;
    sched_group_array[sched_group_id].task_num++;
    spin_unlock_irqrestore(&sched_group_array[sched_group_id].lock, flags);

    return 0;
}

//将一个任务从指定的调度组移除
int period_sched_group_remove (pcb_t pcb, int sched_group_id)
{
    unsigned long flags = 0;

    if (!isValid(sched_group_id))
    {
        return -1;
    }

    spin_lock_irqsave(&sched_group_array[sched_group_id].lock, flags);
    list_del(&pcb->sched_group_node);
    sched_group_array[sched_group_id].task_num--;
    spin_unlock_irqrestore(&sched_group_array[sched_group_id].lock, flags);

    if (0 == sched_group_array[sched_group_id].task_num)
    {
        memset(&sched_group_array[sched_group_id], 0, sizeof(sched_group_array[0]));
    }

    return 0;
}

int period_sched_group_init (void)
{
    int i = 0;
    
    spin_lock_init(&sched_group_lock);

    for (i = 0; i < CONFIG_TASK_GROUP_NUM; i++)
    {
        sched_group_array[i].status = FREE;
        sched_group_array[i].group_id = i; 
    }

    INIT_LIST_HEAD(&wait_actived_task_list);

    return 0;
}

int period_thread_get_unstart_num(void)
{
    struct list_head *node;
    int cnt = 0;
    unsigned long flags = 0;

    spin_lock_irqsave(&sched_group_lock, flags);

    list_for_each(node, &wait_actived_task_list)
    {
        cnt++;
    }

    spin_unlock_irqrestore (&sched_group_lock, flags);
    
    return cnt;
}

INIT_EXPORT_SYS(period_sched_group_init,"period_sched_group_init");
