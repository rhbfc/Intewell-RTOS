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

#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>

/*
 * @brief:
 *    复位信号量到count值，并唤醒对应数量的被挂起的线程
 * @param[in]: semaID: 信号量标识符。
 * @return:
 *          TTOS_OK: 操作成功
 *          TTOS_INVALID_ID:ID为空或者无效的ID或者不是信号量
 * @implements: RTE_DSEMA.1.1
 */
T_TTOS_ReturnCode TTOS_ResetSema (T_WORD count, SEMA_ID semaID)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_SemaControlBlock *sema;

    /* @KEEP_COMMENT: 获取<semaID>指定的信号量存放至变量sema */
    sema = (T_TTOS_SemaControlBlock *)ttosGetObjectById (semaID,
                                                         TTOS_OBJECT_SEMA);

    /* @REPLACE_BRACKET: sema为NULL */
    if (NULL == sema)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: sema为NULL */
    while (sema->semaControlValue < count)
    {
        /* @KEEP_COMMENT: 释放信号量 */
        TTOS_ReleaseSema (semaID);
    }

    /* @KEEP_COMMENT: 使信号量计数值为count */
    sema->semaControlValue = count;

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT:恢复调度，并返回 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
