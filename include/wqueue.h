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

/*
 * @file： wqueue.h
 * @brief：
 *	    <li>工作队列相关函数声明及宏定义。</li>
 */
#ifndef _WQUEUE_H
#define _WQUEUE_H

/************************头文件********************************/
#include <list.h>
#include <stdint.h>
#include <system/types.h>
#include <time/ktime.h>
#include <ttos.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
unsigned int TTOS_GetSysClkRate(void);

#define WORK_QUEUE_PRIORITY 100
#define HIGH_PRI_WORK_QUEUE_PRIORITY 1

/************************类型定义******************************/
#ifndef ASM_USE

typedef void (*worker_t)(void *arg);

struct wqueue
{
    struct list_head wait_queue;
    ttos_spinlock_t lock;
    SEMA_ID wake;
};

struct work_s
{
    struct list_head queue_node;
    T_UDWORD qtime;
    worker_t worker;
    void *arg;
};

#endif
/************************接口声明******************************/
int work_queue(struct work_s *work, worker_t worker, void *arg, T_UDWORD delay,
               struct wqueue *wqueue);
int work_cancel(struct work_s *work, struct wqueue *wqueue);
int work_queue_init(void);
struct wqueue *work_queue_get_highpri(void);

#define work_available(work) ((work)->worker == NULL)

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _WQUEUE_H */
