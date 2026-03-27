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

/************************头 文 件******************************/
#include "ttosUtils.h"
#include <commonTypes.h>
#include <ipi.h>
#include <smp.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "Kernel"
#include <klog.h>

/************************宏 定 义******************************/
/* 任务栈保护 */
#define INIT_MESSAGE KLOG_I
/************************类型定义******************************/
T_TTOS_EventControl *del_task_event;
/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/
/************************全局变量******************************/

/* 处理任务删除自身的任务ID */
T_EXTERN TASK_ID ttosDeleteHandlerTaskID;
T_EXTERN int device_system_init(void);

/************************实   现*******************************/
/*
 * @brief:
 *    任务初始化。
 * @param[in]: arg: 任务参数
 * @return:
 *	  无
 */
static T_VOID ttosInit(T_VOID *arg)
{
    uint32_t irq;

    /*计算频率的时间太长，此处显示调用是为了先将频率计算出来。*/
    (void)TTOS_GetCurrentSystemFreq();

#ifdef CONFIG_SMP
    /*
     *将当前任务设置为全局的，这样后继创建的任务也是全局的。
     *全局任务可以在任意一个核上运行。
     */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    // CPU_SET(0, &cpuset);
    (void)TTOS_SetTaskAffinity(((TASK_ID)TTOS_SELF_OBJECT_ID), &cpuset);
#endif

    /* 创建删除任务 */
    TASK_ID tid;
    del_task_event = TTOS_CreateEvent();
    TTOS_CreateTaskEx((T_UBYTE *)TTOS_DELETE_HANDLER_TASK_NAME, 253, TRUE, TRUE,
                      taskDeleteSelfHandler, NULL, 8192, &tid);

    ttosDeleteHandlerTaskID = tid;

    /* 设置栈释放函数 */
    ttosSetFreeStackRoutine(free);

    ttos_sys_init();

    /* 多核初始化，启动从核 */
    smp_master_init();

    /* 注册重调度IPI */
    irq = ttos_pic_irq_alloc(NULL, GENERAL_IPI_SCHED);
    ttos_pic_irq_install(irq, ipi_reschedule_handler, NULL, 0, "IPI Sched");
}

void ttos_create_init_task(void)
{
    TASK_ID tid;
    TTOS_CreateTaskEx((T_UBYTE *)"ttosInit", 1, TRUE, TRUE, ttosInit, NULL, 0x100000, &tid);
}
