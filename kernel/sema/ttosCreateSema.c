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
#include <ttosBase.h>
#include <ttosInterHal.h>
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
/************************实   现*******************************/
/* @MODULE> */

/*
 * @brief:
 *    判断是否是互斥锁。
 * @param[in]: mutexID: 信号量标识符。
 * @return:
 *          TRUE:是互斥锁
 *          FALSE:ID为空或者无效的ID或者不是互斥锁
 * @implements: RTE_DSEMA.1.1
 */
T_BOOL ttosSemaIsMutex (MUTEX_ID mutexID)
{
    T_TTOS_SemaControlBlock *semaCB;
    /* @KEEP_COMMENT: 获取<semaID>指定的信号量存放至变量sema */
    semaCB = (T_TTOS_SemaControlBlock *)ttosGetObjectById (mutexID,
                                                           TTOS_OBJECT_SEMA);

    /* @REPLACE_BRACKET: sema为NULL */
    if (NULL == semaCB)
    {
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }

    /* @REPLACE_BRACKET: TTOS_MUTEX_SEMAPHORE == (semaCB->attributeSet &
     * TTOS_SEMAPHORE_MASK) */
    if (TTOS_MUTEX_SEMAPHORE == (semaCB->attributeSet & TTOS_SEMAPHORE_MASK))
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TRUE */
        return (TRUE);
    }

    else
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }
}

/*
 * @brief:
 *    初始化信号量。
 * @param[out]||[in]: semaCB:信号量控制块
 * @param[in]: semaConfig: 信号量配置参数
 * @return:
 *    无
 * @implements: RTE_DSEMA.1.2
 */
void ttosInitSema (T_TTOS_SemaControlBlock *semaCB,
                   T_TTOS_ConfigSema *semaConfig, T_BOOL isPriorityCeiling)
{
    T_TTOS_TaskState state = TTOS_TASK_WAITING_FOR_SEMAPHORE;
    /* @KEEP_COMMENT: 根据配置表的属性初始化指定信号量 */
    /* 设置信号量参数 */
    semaCB->objCore.objectNode.next = NULL;
    semaCB->attributeSet            = semaConfig->attributeSet;
    semaCB->isPriorityCeiling       = isPriorityCeiling;
    semaCB->semaControlValue        = semaConfig->initValue;
    semaCB->priorityCeiling         = semaConfig->priorityCeiling;
    semaCB->semaControlId           = (SEMA_ID)semaCB;
    (void)ttosCopyName (semaConfig->cfgSemaName, semaCB->objCore.objName);
    /* @KEEP_COMMENT: 初始状态嵌套深度为零，拥有者为空 */
    semaCB->nestCount              = 0U;
    semaCB->semHolder              = TTOS_NULL_OBJECT_ID;
    semaCB->waitQueue.objectCoreID = &(semaCB->objCore);

    /* @KEEP_COMMENT: 初始化队列 */
    (void)ttosInitializeTaskq (
        &semaCB->waitQueue,
        ((semaCB->attributeSet & TTOS_SEMAPHORE_MASK) == TTOS_MUTEX_SEMAPHORE)
            ? T_TTOS_QUEUE_DISCIPLINE_PRIORITY
            : T_TTOS_QUEUE_DISCIPLINE_FIFO,
        state);
    /* @KEEP_COMMENT: 初始化节点 */
    (void)ttosInitObjectCore (semaCB, TTOS_OBJECT_SEMA);
}

/**
 * @brief:
 *    创建信号量。
 * @param[in]: semaParam: 待创建的信号量参数
 * @param[in/out]: semaID: 创建的信号量标识符
 * @return:
 *    TTOS_CALLED_FROM_ISR: 从中断中调用。
 *    TTOS_INVALID_ADDRESS: semaParam或semaID为空。
 *    TTOS_INVAILD_VERSION: 版本不一致。
 *    TTOS_RESOURCE_IN_USE: semaID指向的资源已被创建。
 *    TTOS_UNSATISFIED: 系统内存不足，分配信号量对象失败。
 *    TTOS_TOO_MANY：系统中正在使用的信号量数已达到用户配置的最大信号量数(静态
 *                                     配置的信号量数+可创建的信号量数)。
 *    TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *    TTOS_INVALID_NUMBER：互斥信号量初始值大于1。
 *    TTOS_INVALID_PRIORITY：天花板优先级小于任务的最低优先级
 *    TTOS_INVALID_ATTRIBUTE：属性不为 TTOS_COUNTING_SEMAPHORE、
 * TTOS_BINARY_SEMAPHORE 和 TTOS_MUTEX_SEMAPHOR 或者 排队策略等错误
 *    TTOS_MUTEX_CEILING_VIOLATED: 创建锁定状态的互斥锁时违反优先级天花板策略，
 *                                     任务优先级高于天花板优先级。
 *    TTOS_OK: 成功创建信号量。
 * @implements: RTE_DSEMA.1.3
 */
T_TTOS_ReturnCode TTOS_CreateSema (T_TTOS_ConfigSema *semaParam,
                                   SEMA_ID           *semaID)
{
    TBSP_MSR_TYPE            msr    = 0U;
    T_TTOS_SemaControlBlock *semaCB = NULL;
    T_BOOL                   status;
    T_UWORD                  isPriorityCeiling;
    T_TTOS_ReturnCode        ret;
    T_TTOS_ReturnCode        switch_ret = TTOS_OK;

    /* @KEEP_COMMENT:创建信号量不允许在中断中 */
    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT:配置参数为空 */
    /* @REPLACE_BRACKET: NULL == semaParam */
    if (NULL == semaParam)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:判断版本是否一致 */
    /* @REPLACE_BRACKET: FALSE ==
     * ttosCompareVerison(semaParam->objVersion,(T_UBYTE
     * *)TTOS_OBJ_CONFIG_CURRENT_VERSION) */
    if (FALSE
        == ttosCompareVerison (semaParam->objVersion,
                               (T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION))
    {
        /* @REPLACE_BRACKET: TTOS_INVAILD_VERSION */
        return (TTOS_INVAILD_VERSION);
    }

    /* @KEEP_COMMENT: 信号ID为空 */
    /* @REPLACE_BRACKET: NULL == semaID */
    if (NULL == semaID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:判断名字字符串是否符合命名规范 */
    status = ttosVerifyName (semaParam->cfgSemaName, TTOS_OBJECT_NAME_LENGTH);

    /* @REPLACE_BRACKET: FALSE == status */
    if (FALSE == status)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NAME */
        return (TTOS_INVALID_NAME);
    }

    /* 将优先级天花板状态置为假 */
    isPriorityCeiling = FALSE;

    /* @KEEP_COMMENT: 根据信号量类型进行检查 */
    /* @REPLACE_BRACKET: semaParam->attributeSet & TTOS_SEMAPHORE_MASK */
    switch (semaParam->attributeSet & TTOS_SEMAPHORE_MASK)
    {
    /* @KEEP_COMMENT: 计数信号量，直接跳出 */
    case TTOS_COUNTING_SEMAPHORE:
        break;

    /* @KEEP_COMMENT: 互斥量，初始值不能够大于1，并且判定优先级天花板的有效性 */
    case TTOS_MUTEX_SEMAPHORE:

        /* @REPLACE_BRACKET: semaParam->initValue > U(1) */
        if (semaParam->initValue > 1U)
        {
            /* @KEEP_COMMENT: switch_ret设置为TTOS_INVALID_NUMBER */
            switch_ret = TTOS_INVALID_NUMBER;
            break;
        }

        /* @KEEP_COMMENT: 天花板优先级不合法返回无效优先级状态值 */
        /* @REPLACE_BRACKET: semaParam->priorityCeiling > TTOS_MAX_PRIORITY*/
        if (semaParam->priorityCeiling > TTOS_MAX_PRIORITY)
        {
            /* @KEEP_COMMENT: switch_ret设置为TTOS_INVALID_PRIORITY */
            switch_ret = TTOS_INVALID_PRIORITY;
            break;
        }

	    /* @KEEP_COMMENT: 仅在用户指定优先级天花板策略时，使能优先级天花板策略 */
        if((semaParam->attributeSet & TTOS_MUTEX_WAIT_ATTR_MASK) == TTOS_MUTEX_WAIT_PRIORITY_CEILING)
        {
            /* @KEEP_COMMENT: 天花板优先级不合法返回无效优先级状态值 */
            if(semaParam->priorityCeiling > TTOS_MAX_PRIORITY)
            {
                return(TTOS_INVALID_PRIORITY);
            }

            isPriorityCeiling = TRUE;
        }

        break;

    /* @KEEP_COMMENT: 无效的信号量类型返回无效属性状态值 */
    default:
        /* @KEEP_COMMENT: switch_ret设置为TTOS_INVALID_ATTRIBUTE */
        switch_ret = TTOS_INVALID_ATTRIBUTE;
        break;
    }

    /* @REPLACE_BRACKET: switch_ret != TTOS_OK */
    if (switch_ret != TTOS_OK)
    {
        /* @REPLACE_BRACKET: switch_ret */
        return (switch_ret);
    }

    (void)ttosDisableTaskDispatchWithLock ();
    ret = ttosAllocateObject (TTOS_OBJECT_SEMA, (T_VOID **)&semaCB);

    /* @REPLACE_BRACKET: TTOS_OK != ret */
    if (TTOS_OK != ret)
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: ret */
        return (ret);
    }

    /* @KEEP_COMMENT:  禁止中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    ttosInitSema (semaCB, semaParam, isPriorityCeiling);
    /* @KEEP_COMMENT:  使能中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    /* @KEEP_COMMENT:  输出信号量对象ID */
    *semaID = semaCB->semaControlId;
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
