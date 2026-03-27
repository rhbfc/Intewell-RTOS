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

/* @<MODULE */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <cpu.h>
#include <ttosHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/* @<MOD_EXTERN */
T_EXTERN void ttosDisableTaskDispatchWithLock (void);

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/

/* @<MOD_VAR */

/* 是否栈填充*/
T_BOOL ttosTaskStackFilledFlag;

/* @MOD_VAR> */

/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定任务的栈信息。
 * @param[in]: taskID: 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务的栈信息
 * @param[out]: taskStackInfo: 存放任务的栈信息
 * @return:
 *    TTOS_INVALID_TYPE：不允许获取任务栈信息；
 *    TTOS_INVALID_ID：<taskID>所指定的对象类型不是任务或不存在。
 *    TTOS_INVALID_ADDRESS：<taskStackInfo>为NULL。
 *    TTOS_OK：获取任务的栈信息成功。
 * @note: 返回栈信息并不是全部有效，无效信息值为TTOS_INFO_INVALID
 * @implements: RTE_DTASK.20.1
 */
T_TTOS_ReturnCode
TTOS_GetTaskStackInfo (TASK_ID                      taskID,
                       T_TTOS_TaskStackInformation *taskStackInfo)
{
    T_TTOS_TaskControlBlock *task;
    T_UWORD                 *stackMaxUsrPtr = NULL;

    /* @REPLACE_BRACKET: <taskStackInfo>为空 */
    if (NULL == taskStackInfo)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        (void)ttosDisableTaskDispatchWithLock ();
        /* @KEEP_COMMENT: 获取正在运行的任务ttosManager.runningTask存放至<task>
         */
        task = ttosGetRunningTask ();
        if (NULL == task)
        {
            return (TTOS_INVALID_ID);
        }
    }

    else
    {
        /* @KEEP_COMMENT: 通过<taskID>获取相应任务的任务控制块 */
        task = (T_TTOS_TaskControlBlock *)ttosGetObjectById (taskID,
                                                             TTOS_OBJECT_TASK);

        /* @REPLACE_BRACKET: <taskID>指定的任务不存在 */
        if (NULL == task)
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_ID */
            return (TTOS_INVALID_ID);
        }
    }

    /* @KEEP_COMMENT: 将任务的栈信息存放在栈信息结构体<taskStackInfo>中 */
    taskStackInfo->stackBase   = task->kernelStackTop;
    taskStackInfo->stackBottom = task->stackBottom;
    taskStackInfo->stackStart  = task->stackStart;
    taskStackInfo->stackSize   = task->kernelStackTop - task->stackBottom;
    taskStackInfo->isFreeStack = task->isFreeStack;

    /* 获取<taskID>指定任务的当前栈指针 */

    /* @REPLACE_BRACKET: 任务控制块<task>为当前正在运行任务的任务控制块 */
    if (TRUE == ttosIsRunningTask (task))
    {
        /* @KEEP_COMMENT: 通过栈信息结构体<taskStackInfo>获取 任务当前栈指针*/
        GET_CURRENT_STATCK (taskStackInfo->currentStack);
    }

    else
    {
        /* @KEEP_COMMENT: 通过任务控制块<task>中的任务上下文获取
         * 任务当前栈指针*/
        taskStackInfo->currentStack
            = (T_UBYTE *)((T_ULONG)task->switchContext.sp);
    }

    /* @REPLACE_BRACKET: 当前栈指针大于栈顶指针 */
    if (taskStackInfo->currentStack > taskStackInfo->stackBase)
    {
        /* @KEEP_COMMENT: 栈向上溢出 */
        taskStackInfo->isStackOverflowed = TRUE;
    }

    else
    {
        /* @KEEP_COMMENT: 栈未向上溢出 */
        taskStackInfo->isStackOverflowed = FALSE;
    }

    /* @REPLACE_BRACKET: 当前栈指针小于栈底指针 */
    if (taskStackInfo->currentStack < taskStackInfo->stackBottom)
    {
        /* @KEEP_COMMENT: 栈向下溢出 */
        taskStackInfo->isStackUnderflowed = TRUE;
    }

    else
    {
        /* @KEEP_COMMENT: 栈未向下溢出 */
        taskStackInfo->isStackUnderflowed = FALSE;
    }

    /* @REPLACE_BRACKET: <taskStackInfo>获取栈信息使能 */
    if ((TRUE == ttosTaskStackFilledFlag))
    {
        /* 获取栈使用过的边界地址 */
        stackMaxUsrPtr = (T_UWORD *)taskStackInfo->currentStack;

        /* @REPLACE_BRACKET: 当前栈指针大于任务运行栈栈底指针且栈没有发生溢出 */
        if ((FALSE == taskStackInfo->isStackOverflowed)
            && (FALSE == taskStackInfo->isStackUnderflowed))
        {
            /* @KEEP_COMMENT:检查任务栈的使用情况时，必须考虑当任务还未运行时的情况，只有当任务运行过，才需要做如下检查
             */
            /* @REPLACE_BRACKET: (T_UBYTE *)stackMaxUsrPtr !=
             * taskStackInfo->stackBase */
            if ((T_UBYTE *)stackMaxUsrPtr != taskStackInfo->stackBase)
            {
                /* @REPLACE_BRACKET:
                 * 当前栈指针副本<stackMaxUsrPtr>大于任务运行栈栈底指针 */
                while ((T_ULONG)stackMaxUsrPtr
                       >= (T_ULONG)taskStackInfo->stackBottom)
                {
                    /* @REPLACE_BRACKET:
                     * 当前栈指针副本<stackMaxUsrPtr>到达栈边界 */
                    if (0xababababU == *(T_UWORD *)stackMaxUsrPtr)
                    {
                        break;
                    }

                    /* @KEEP_COMMENT: 当前栈指针副本<stackMaxUsrPtr>减1 */
                    stackMaxUsrPtr--;
                }

                /* @REPLACE_BRACKET:
                 * 当前栈指针副本<stackMaxUsrPtr>指向的空间没有被使用 */
                if (0xababababU == *(T_UWORD *)stackMaxUsrPtr)
                {
                    /* @KEEP_COMMENT:
                     * 使栈指针副本<stackMaxUsrPtr>准确指向被使用的空间 */
                    stackMaxUsrPtr++;
                }
            }

            /* @KEEP_COMMENT:
             * 保存任务运行时使用的最大栈的大小至栈信息结构<taskStackInfo>中，其值为任务运行栈基地址与当前栈指针副本<stackMaxUsrPtr>的差
             */
            taskStackInfo->stackMaxUseSize
                = (T_ULONG)taskStackInfo->stackBase - (T_ULONG)stackMaxUsrPtr;
        }

        else
        {
            /* @KEEP_COMMENT:
             * 保存任务运行时使用的最大栈的大小至栈信息结构<taskStackInfo>中，其值为栈的大小
             */
            taskStackInfo->stackMaxUseSize = taskStackInfo->stackSize;
        }
    }

    else
    {
        /* 未使能栈标记，无法获取栈历史高位值，返回该值不可用 */
        taskStackInfo->stackMaxUseSize = TTOS_INVALID_VALUE;
    }

    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
