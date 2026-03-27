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
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <ttos.h>
#include <wqueue.h>

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
struct wqueue default_work_queue;
struct wqueue high_pri_work_queue;

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************函数实现******************************/
/*
 * @brief:
 *    获取高优先级工作队列。
 * @param[in]: 无
 * @return:
 *    高优先级工作队列指针
 */
struct wqueue *work_queue_get_highpri(void)
{
    return &high_pri_work_queue;
}
/*
 * @brief:
 *    加入工作队列。
 * @param[in]: wqueue 全局工作队列控制结构
 * @param[in]: work   指定的工作队列控制结构
 * @param[in]: worker 工作队列回调函数
 * @param[in]: arg 工作队列回调函数参数
 * @param[in]: delay 延迟生效的微秒数
 * @return:
 *    ENONE 成功
 */
static int work_queue_add(struct wqueue *wqueue, struct work_s *work, worker_t worker, void *arg,
                          T_UDWORD delay_ticks)
{
    int sem_count;
    struct list_head *node;
    unsigned long long delta = 0;
    unsigned long flags = 0;
    T_UDWORD now_ticks = 0;

    INIT_LIST_HEAD(&work->queue_node);
    work->worker = worker;
    work->arg = arg;

    TTOS_GetSystemTicks(&now_ticks);
    work->qtime = now_ticks + delay_ticks;

    spin_lock_irqsave(&wqueue->lock, flags);

    if (list_empty(&wqueue->wait_queue))
    {
        list_add(&work->queue_node, &wqueue->wait_queue);
        spin_unlock_irqrestore(&wqueue->lock, flags);
        TTOS_ReleaseSema(wqueue->wake);
        return 0;
    }

    list_for_each(node, &wqueue->wait_queue)
    {
        delta = work->qtime - ((struct work_s *)node)->qtime;
        if (delta < 0)
        {
            /* 找到特定节点，插入到该节点之前 */
            break;
        }
    }

    /* 链表遍历完毕,未找到特定节点 */
    if (list_is_head(node, &wqueue->wait_queue))
    {
        /* 加入到链表尾 */
        list_add_tail(&work->queue_node, &wqueue->wait_queue);
        spin_unlock_irqrestore(&wqueue->lock, flags);
    }
    else
    {
        /* 找到特定节点，插入到该节点之前 */
        list_add(&work->queue_node, node->prev);
        spin_unlock_irqrestore(&wqueue->lock, flags);

        /* 插入到第一个节点 */
        if (list_is_first(&work->queue_node, &wqueue->wait_queue))
        {
            sem_count = wqueue->wake->semaControlValue;
            if (sem_count < 1)
            {
                TTOS_ReleaseSema(wqueue->wake);
            }
        }
    }

    return 0;
}

/*
 * @brief:
 *    添加工作队列。
 * @param[in]: work 指定的工作队列控制结构
 * @param[in]: worker 工作队列回调函数
 * @param[in]: arg 工作队列回调函数参数
 * @param[in]: delay 延迟生效时间
 * @return:
 *    ENONE 成功
 */
int work_queue(struct work_s *work, worker_t worker, void *arg, T_UDWORD delay,
               struct wqueue *wqueue)
{
    if (!wqueue)
    {
        wqueue = &default_work_queue;
    }

    work_cancel(work, wqueue);

    return work_queue_add(wqueue, work, worker, arg, delay);
}
