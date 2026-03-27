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
T_EXTERN void ttosDisableTaskDispatchWithLock (void);
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/
/* @MODULE> */

/*
 * @brief:
 *    初始化定时器。
 * @param[out]||[in]: timerCB:定时器控制块
 * @param[in]: timerConfig: 定时器配置参数
 * @return:
 *    无
 * @implements: RTE_DTIMER.2
 */
void ttosInitTimer (T_TTOS_TimerControlBlock *timerCB,
                    T_TTOS_ConfigTimer       *timerConfig)
{
    /* @KEEP_COMMENT: 根据配置表的属性初始化指定定时器 */
    timerCB->objCore.objectNode.next = NULL;
    timerCB->timerControlId          = (TIMER_ID)timerCB;
    (void)ttosCopyName (timerConfig->cfgTimerName, timerCB->objCore.objName);
    timerCB->ticks       = timerConfig->ticks;
    timerCB->repeatCount = timerConfig->repeatCount;
    timerCB->argument    = timerConfig->argument;
    timerCB->handler     = timerConfig->handler;

    /* @REPLACE_BRACKET: 定时器i开启自动激活 */
    if (TRUE == timerConfig->autoStarted)
    {
        /* @KEEP_COMMENT: 设置定时器i状态为TTOS_TIMER_ACTIVE */
        timerCB->state = (T_UWORD)TTOS_TIMER_ACTIVE;
        /* @KEEP_COMMENT: 配置定时器i的信息 */
        timerCB->remainCount = timerCB->repeatCount;
        timerCB->waitedTicks = timerCB->ticks;
        /*
         * @KEEP_COMMENT: 调用ttosInsertWaitedTimer(DT.4.9)将定时器插入定时器
         * 等待队列
         */
        (void)ttosInsertWaitedTimer (timerCB);
    }

    else
    {
        /* 定时时间和触发时的处理程序必须是有效的数据 */
        /* @REPLACE_BRACKET: 定时时间不为0 && 触发时的处理程序不为0 */
        if ((timerCB->ticks != 0U) && (timerCB->handler != NULL))
        {
            /* @KEEP_COMMENT: 设置定时器i状态为TTOS_TIMER_INSTALLED */
            timerCB->state = (T_UWORD)TTOS_TIMER_INSTALLED;
        }

        else
        {
            /* @KEEP_COMMENT: 设置定时器i状态为TTOS_TIMER_UNINSTALL */
            timerCB->state = (T_UWORD)TTOS_TIMER_UNINSTALL;
        }
    }

    (void)ttosInitObjectCore (timerCB, TTOS_OBJECT_TIMER);
}

/**
 * @brief:
 *    创建定时器。
 * @param[in]: timerParam: 待创建的定时器参数
 * @param[out]: timerID: 创建的定时器标识符
 * @return:
 *    TTOS_CALLED_FROM_ISR: 从中断中调用。
 *    TTOS_INVALID_ADDRESS:
 * timerParam或timerID或自启动定时器处理函数入口地址为NULL。
 *    TTOS_INVAILD_VERSION:版本不一致。
 *    TTOS_INVALID_NUMBER: 自启动定时器定时器被触发的时间间隔为0。
 *    TTOS_TOO_MANY: 系统中正在使用的定时器数已达到用户配置的最大定时器数(静态
 *                                  配置的定时器数+可创建的定时器数)。
 *    TTOS_UNSATISFIED: 分配定时器对象失败。
 *    TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *    TTOS_OK: 成功创建定时器。
 * @implements: RTE_DTIMER.4
 */
T_TTOS_ReturnCode TTOS_CreateTimer (T_TTOS_ConfigTimer *timerParam,
                                    TIMER_ID           *timerID)
{
    TBSP_MSR_TYPE             msr     = 0U;
    T_TTOS_TimerControlBlock *timerCB = NULL;
    T_BOOL                    status;
    T_TTOS_ReturnCode         ret;

    /* @REPLACE_BRACKET:  TRUE == ttosIsISR()*/
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET:  TTOS_CALLED_FROM_ISR*/
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET:  NULL == timerParam*/
    if (NULL == timerParam)
    {
        /* @REPLACE_BRACKET:  TTOS_INVALID_ADDRESS*/
        return (TTOS_INVALID_ADDRESS);
    }

    /*判断版本是否一致*/
    /* @REPLACE_BRACKET:  比较版本是否相等*/
    if (FALSE
        == ttosCompareVerison (timerParam->objVersion,
                               (T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION))
    {
        /* @REPLACE_BRACKET:  TTOS_INVAILD_VERSION*/
        return (TTOS_INVAILD_VERSION);
    }

    /* @REPLACE_BRACKET:  NULL == timerID*/
    if (NULL == timerID)
    {
        /* @REPLACE_BRACKET:  TTOS_INVALID_ADDRESS*/
        return (TTOS_INVALID_ADDRESS);
    }

    /*判断名字字符串是否符合命名规范*/
    status = ttosVerifyName (timerParam->cfgTimerName, TTOS_OBJECT_NAME_LENGTH);

    /* @REPLACE_BRACKET:  FALSE == status*/
    if (FALSE == status)
    {
        /* @REPLACE_BRACKET:  TTOS_INVALID_NAME*/
        return (TTOS_INVALID_NAME);
    }

    /* @REPLACE_BRACKET: 定时器开启自动激活 */
    if (TRUE == timerParam->autoStarted)
    {
        /* @REPLACE_BRACKET: 定时器的定时器时间间隔tick为0 */
        if (0U == timerParam->ticks)
        {
            /* @REPLACE_BRACKET:  TTOS_INVALID_NUMBER*/
            return (TTOS_INVALID_NUMBER);
        }

        /* @REPLACE_BRACKET: 定时器i的处理函数为空或者定时器时间间隔tick为0 */
        if (NULL == timerParam->handler)
        {
            /* @REPLACE_BRACKET:  TTOS_INVALID_ADDRESS*/
            return (TTOS_INVALID_ADDRESS);
        }
    }

    (void)ttosDisableTaskDispatchWithLock ();
    ret = ttosAllocateObject (TTOS_OBJECT_TIMER, (T_VOID **)&timerCB);

    /* @REPLACE_BRACKET:  TTOS_OK != ret*/
    if (TTOS_OK != ret)
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET:  ret*/
        return (ret);
    }

    /* 禁止中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    ttosInitTimer (timerCB, timerParam);
    /* 使能中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    /* 输出定时器对象ID */
    *timerID = timerCB->timerControlId;
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET:  TTOS_OK*/
    return (TTOS_OK);
}
