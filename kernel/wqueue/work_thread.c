/****************************************************************************
 * sched/wqueue/kwork_thread.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/************************头 文 件******************************/
#define _GNU_SOURCE 1

#include <assert.h>
#include <commonUtils.h>
#include <errno.h>
#include <list.h>
#include <sched.h>
#include <spinlock.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ttos.h>
#include <ttos_init.h>
#include <unistd.h>
#include <wqueue.h>

#define KLOG_TAG "WorkQueue"
#include <klog.h>

/************************宏 定 义******************************/
#define WORK_QUEUE_STACKSIZE 0x8000
#define WORK_QUEUE_DELAY_MAX UINT64_MAX
#define NSEC_PER_TICK (NSEC_PER_SEC / TTOS_GetSysClkRate())

/************************类型定义******************************/
/************************外部声明******************************/
extern struct wqueue default_work_queue;
extern struct wqueue high_pri_work_queue;

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************函数实现******************************/

/*
 * @brief:
 *    工作队列处理。
 * @param[in]: wqueue 工作队列
 * @return:
 *    无
 */
static void work_process(struct wqueue *wqueue)
{
    struct work_s *node;
    struct work_s *tmp_node;
    volatile struct work_s *work;
    worker_t worker;
    void *arg;
    unsigned long long elapsed;
    T_UDWORD next;
    T_UDWORD now_ticks;
    int ret;
    long flags;

    next = WORK_QUEUE_DELAY_MAX;

    spin_lock_irqsave(&wqueue->lock, flags);

retry:
    list_for_each_entry_safe(tmp_node, node, &wqueue->wait_queue, queue_node)
    {
        /* 获取当前时间,计算节点是否超时 */
        TTOS_GetSystemTicks(&now_ticks);

        /* 当前时刻点已经到达/超过期望时刻 */
        if (now_ticks >= tmp_node->qtime)
        {
            list_del(&tmp_node->queue_node);

            worker = tmp_node->worker;

            if (worker != NULL)
            {
                arg = tmp_node->arg;

                tmp_node->worker = NULL;

                spin_unlock_irqrestore(&wqueue->lock, flags);

                worker(arg);

                spin_lock_irqsave(&wqueue->lock, flags);
            }
            else
            {
                continue;
            }

            /* 由于调用handler前释放了自旋锁，并且handler中可能有节点加入,因此，需要重新遍历
             */
            goto retry;
        }
        else
        {
            /* 计算下一个节点多久之后触发 */
            TTOS_GetSystemTicks(&now_ticks);
            next = tmp_node->qtime - now_ticks;
            break;
        }
    }

    spin_unlock_irqrestore(&wqueue->lock, flags);

    if (next == WORK_QUEUE_DELAY_MAX)
    {
        /* 队列中没有节点,永久等待,直到有新节点加入 */
        TTOS_ObtainSema(wqueue->wake, TTOS_WAIT_FOREVER);
    }
    else
    {
        /* 设置下一个节点的时间为超时等待时间 */
        TTOS_ObtainSema(wqueue->wake, next);
    }
}

/*
 * @brief:
 *    工作队列线程。
 * @param[in]: arg 线程参数
 * @return:
 *    NULL
 */
static void *work_queue_thread(void *arg)
{
    while (1)
    {
        work_process((struct wqueue *)arg);
    }

    return NULL;
}

/*
 * @brief:
 *    创建工作队列。
 * @param[in]: 无
 * @return:
 *    0 成功
 *    非0 失败
 */
int work_queue_create(struct wqueue *queue, T_UBYTE priority)
{
    TASK_ID thread_id;

    INIT_LIST_HEAD(&queue->wait_queue);
    spin_lock_init(&queue->lock);
    TTOS_CreateSemaEx(0, &queue->wake);

    TTOS_CreateTaskEx((T_UBYTE *)"work queue", priority, TRUE, TRUE,
                      (T_TTOS_TaskRoutine)work_queue_thread, queue, WORK_QUEUE_STACKSIZE,
                      &thread_id);

    return 0;
}

/*
 * @brief:
 *    工作队列初始化。
 * @param[in]: 无
 * @return:
 *    0 成功
 *    非0 失败
 */
int work_queue_init(void)
{
    work_queue_create(&default_work_queue, WORK_QUEUE_PRIORITY);
    work_queue_create(&high_pri_work_queue, HIGH_PRI_WORK_QUEUE_PRIORITY);

    return 0;
}

INIT_EXPORT_SYS(work_queue_init, "work_queue_init");
