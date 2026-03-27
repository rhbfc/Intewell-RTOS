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
#include <list.h>
#include <wqueue.h>

#define KLOG_TAG "work_queue"
#include <klog.h>

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
extern struct wqueue default_work_queue;

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************函数实现******************************/
/*
 * @brief:
 *    取消指定的工作队中的指定节点。
 * @param[in]: wqueue 指定的工作队列
 * @param[in]: work 指定的工作队列控制结构
 * @return:
 *    0 成功
 *    非0 失败
 */
static int work_qcancel(struct wqueue *wqueue, struct work_s *work)
{
    int is_first_node = false;
    struct list_head *node;
    int ret = -ENOENT;
    int semcount;
    long flags = 0;

    if (!wqueue || !work)
    {
        return -EINVAL;
    }

    spin_lock_irqsave(&wqueue->lock, flags);

    if (work->worker == NULL)
    {
        spin_unlock_irqrestore(&wqueue->lock, flags);
        return 0;
    }

    list_for_each(node, &wqueue->wait_queue)
    {
        if (node == &work->queue_node)
        {
            if (list_is_first(node, &work->queue_node))
            {
                is_first_node = true;
            }

            list_del(node);

            break;
        }
    }

    /* 在链表中未找到 */
    if (list_is_head(node, &wqueue->wait_queue))
    {
        KLOG_E("fail at %s:%d", __FILE__, __LINE__);
        spin_unlock_irqrestore(&wqueue->lock, flags);
        return -EINVAL;
    }

    if (is_first_node)
    {
        semcount = wqueue->wake->semaControlValue;
        if (semcount < 1)
        {
            TTOS_ReleaseSema(wqueue->wake);
        }
    }

    ret = 0;
    work->worker = NULL;

    spin_unlock_irqrestore(&wqueue->lock, flags);

    return ret;
}

/*
 * @brief:
 *    指定的工作队列节点是否已经存在。
 * @param[in]: wqueue 指定的工作队列
 * @param[in]: work 指定的工作队列控制结构
 * @return:
 *    true 存在
 *    false 不存在
 */
bool work_is_exist(struct wqueue *wqueue, struct work_s *work)
{
    struct list_head *node;

    list_for_each(node, &wqueue->wait_queue)
    {
        if (node == &work->queue_node)
        {
            return true;
        }
    }

    return false;
}

/*
 * @brief:
 *    取消指定的工作队列节点。
 * @param[in]: work 指定的工作队列控制结构
 * @return:
 *    0 成功
 *    非0 失败
 */
int work_cancel(struct work_s *work, struct wqueue *queue)
{
    long flags = 0;

    if (!queue)
    {
        queue = &default_work_queue;
    }

    spin_lock_irqsave(&queue->lock, flags);

    if (!work_is_exist(queue, work))
    {
        spin_unlock_irqrestore(&queue->lock, flags);
        return EINVAL;
    }

    spin_unlock_irqrestore(&queue->lock, flags);

    return work_qcancel(queue, work);
}
