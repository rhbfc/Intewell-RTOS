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
#ifdef CONFIG_PROTECT_STACK

#include <ttosBase.h>
#include <ttosMM.h>

#define KLOG_TAG "TASK"
#include <klog.h>

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************函数实现******************************/

T_TTOS_ReturnCode ttosStackUnprotect (TASK_ID taskID)
{
    struct T_TTOS_TaskControlBlock_Struct *pTask;
    T_TTOS_ReturnCode                      ret = TTOS_OK;

    /* @REPLACE_BRACKET: TTOS_SELF_OBJECT_ID == taskID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT: 获取当前正在执行的任务ID存放至变量pTask */
        taskID = ttosGetRunningTask ()->taskControlId;
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量pTask */
    pTask = (T_TTOS_TaskControlBlock *)ttosGetObjectById (taskID,
                                                          TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: 任务栈底与任务栈起始地址相同 */
    if (pTask->stackBottom == pTask->stackStart)
    {
        KLOG_E ("%s: task not protect stack", __func__);

        /* @KEEP_COMMENT: 设置返回值为 TTOS_INVALID_TYPE */
        ret = TTOS_INVALID_TYPE;

        /* @KEEP_COMMENT: 跳转到退出标签 */
        goto out;
    }

    /* @KEEP_COMMENT: 任务栈底恢复原值 */
    pTask->stackBottom = pTask->stackStart;

    /* @KEEP_COMMENT: 任务栈起始地址的一个页设置为正常访问 */
    ret = ttosSetPageAttribute ((virt_addr_t)pTask->stackStart, PAGE_SIZE,
                                MT_RWX_DATA | MT_KERNEL);

out:
    ttosEnableTaskDispatchWithLock ();
    return ret;
}

#endif /* CONFIG_PROTECT_STACK */
