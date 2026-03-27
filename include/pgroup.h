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
 * @file： pgroup.h
 * @brief：
 *	    <li>进程组相关函数声明及宏定义。</li>
 */
#ifndef _PGROUP_H
#define _PGROUP_H

/************************头文件********************************/
#include <list.h>
#include <stdint.h>
#include <system/types.h>
#include <ttos.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
unsigned int TTOS_GetSysClkRate(void);

#define WORK_QUEUE_PRIORITY 100

/************************类型定义******************************/
#ifndef ASM_USE

struct pgroup
{
    /* 进程组leader进程 */
    pcb_t leader;

    /* 全局进程组节点 */
    struct list_head global_pgroup_node;

    /* 进程组中的进程链表 */
    struct list_head process;

    /* 进程组的id */
    pid_t pgid;

    pid_t sid;

    /* 保护该数据结构的锁 */
    MUTEX_ID lock;

    atomic_t ref;

    /* flags on process group */
    unsigned int is_orphaned : 1;
};

typedef struct pgroup *pgroup_t;
typedef void (*foreach_pgrp_func)(pcb_t pcb, void *param);

#endif
/************************接口声明******************************/
pgroup_t process_pgrp_find(pid_t pgid);
pgroup_t pgrp_create(pcb_t leader);
int pgrp_insert(pgroup_t group, pcb_t process);
int pgrp_init(void);
pid_t process_pgid_get_byprocess(pcb_t process);
void pgrp_put(pgroup_t group);
bool pgrp_remove(pgroup_t group, pcb_t process);
void foreach_pgrp_process(foreach_pgrp_func func, pgroup_t group, void *param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _PGROUP_H */
