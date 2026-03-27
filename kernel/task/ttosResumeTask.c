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

/************************Í· ÎÄ ¼þ******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************ºê ¶¨ Òå******************************/
/************************ÀàÐÍ¶¨Òå******************************/
/************************Íâ²¿ÉùÃ÷******************************/
/************************Ç°ÏòÉùÃ÷******************************/
/************************Ä£¿é±äÁ¿******************************/
/************************È«¾Ö±äÁ¿******************************/
/************************Êµ   ÏÖ*******************************/

/* @MODULE> */

/**
 * @brief:
 *    ½â¹ÒÖ¸¶¨µÄÈÎÎñ¡£
 * @param[in]: taskID: ÈÎÎñµÄID
 * @return:
 *    TTOS_INVALID_ID£º<taskID>Ëù±íÊ¾µÄ¶ÔÏóÀàÐÍ²»ÊÇÈÎÎñ£»
 *                     ²»´æÔÚ<taskID>Ö¸¶¨µÄÈÎÎñ¡£
 *    TTOS_INVALID_STATE£º<taskID>Ö¸¶¨µÄÈÎÎñÎ´±»¹ÒÆð¡£
 *    TTOS_OK£º½â¹ÒÖ¸¶¨µÄÈÎÎñ³É¹¦¡£
 * @implements: RTE_DTASK.30.1
 */
T_TTOS_ReturnCode TTOS_ResumeTask(TASK_ID taskID)
{
    TBSP_MSR_TYPE msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_UWORD nonRunState = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    T_UWORD suspendState = (T_UWORD)TTOS_TASK_SUSPEND;
    /* @KEEP_COMMENT: »ñÈ¡<taskID>Ö¸¶¨µÄÈÎÎñ´æ·ÅÖÁ±äÁ¿task */
    task = (T_TTOS_TaskControlBlock *)ttosGetObjectById(taskID, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: taskÈÎÎñ²»´æÔÚ */
    if ((0ULL) == task)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: ½ûÖ¹ÐéÄâÖÐ¶Ï */
    TBSP_GLOBALINT_DISABLE(msr);

    /* @REPLACE_BRACKET: taskÈÎÎñ×´Ì¬²»ÎªTTOS_TASK_SUSPEND */
    if (suspendState != (task->state & suspendState))
    {
        /* @KEEP_COMMENT: Ê¹ÄÜÐéÄâÖÐ¶Ï */
        TBSP_GLOBALINT_ENABLE(msr);
        (void)ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT: Çå³ýtaskÈÎÎñµÄTTOS_TASK_SUSPEND×´Ì¬ */
    task->state &= (~suspendState);

    /* @REPLACE_BRACKET: taskÈÎÎñµÄ×´Ì¬²»ÎªTTOS_TASK_NONRUNNING_STATE */
    if (0 == (task->state & nonRunState))
    {
        /* @KEEP_COMMENT: µ÷ÓÃttosSetTaskReady(DT.2.32)ÉèÖÃ<task>ÈÎÎñÎª¾ÍÐ÷ÈÎÎñ
         */
        (void)ttosSetTaskReady(task);
    }

    /* @KEEP_COMMENT: Ê¹ÄÜÐéÄâÖÐ¶Ï */
    TBSP_GLOBALINT_ENABLE(msr);
    (void)ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

T_TTOS_ReturnCode TTOS_SignalResumeTask(TASK_ID taskID)
{
    T_TTOS_ReturnCode ret;

    (void)ttosDisableTaskDispatchWithLock();
    taskID->state &= (~TTOS_TASK_STOPPED_BY_SIGNAL);
    ret = TTOS_ResumeTask(taskID);
    (void)ttosEnableTaskDispatchWithLock();
    return ret;
}

T_TTOS_ReturnCode task_delete_resume(TASK_ID taskID)
{
    T_TTOS_ReturnCode ret;

    (void)ttosDisableTaskDispatchWithLock();
    taskID->state &= (~TTOS_TASK_WAITING_DELETE);
    ret = TTOS_ResumeTask(taskID);
    (void)ttosEnableTaskDispatchWithLock();

    return ret;
}