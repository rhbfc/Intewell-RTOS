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

/* 头文件 */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>

/************************前向声明******************************/
/* @MOD_HEAD> */

/*实现*/
/* @MODULE> */

/*
 * @brief
 *       获取内核对象数量信息
 * @param[out]   kernelInfo: 内核相关信息
 * @return
 *    TTOS_INVALID_ADDRESS：传入参数地址为空
 *    TTOS_OK:获取信息成功
 * @implements: RTE_DUTILS.4.1
 */
T_TTOS_ReturnCode TTOS_GetKernelInfo (T_TTOS_KernelBasicInfo *kernelInfo)
{
    /* @REPLACE_BRACKET: kernelInfo == NULL */
    if (kernelInfo == NULL)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:  获取任务个数的相关信息 */
    kernelInfo->taskMaxNumber = ttosManager.objMaxNumber[TTOS_OBJECT_TASK];
    kernelInfo->taskInusedNumber
        = ttosManager.inUsedResourceNum[TTOS_OBJECT_TASK];
    ;
    /* @KEEP_COMMENT:  获取信号量个数的相关信息 */
    kernelInfo->semaMaxNumber = ttosManager.objMaxNumber[TTOS_OBJECT_SEMA];
    kernelInfo->semaInusedNumber
        = ttosManager.inUsedResourceNum[TTOS_OBJECT_SEMA];
    /* @KEEP_COMMENT:  获取定时器个数的相关信息 */
    kernelInfo->timerMaxNumber = ttosManager.objMaxNumber[TTOS_OBJECT_TIMER];
    kernelInfo->timerInusedNumber
        = ttosManager.inUsedResourceNum[TTOS_OBJECT_TIMER];
    /* @KEEP_COMMENT:  获取消息队列个数的相关信息 */
    kernelInfo->msgqMaxNumber = ttosManager.objMaxNumber[TTOS_OBJECT_MSGQ];
    kernelInfo->msgqInusedNumber
        = ttosManager.inUsedResourceNum[TTOS_OBJECT_MSGQ];

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
