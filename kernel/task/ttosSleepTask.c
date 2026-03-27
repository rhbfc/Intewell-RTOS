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

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************Ç°ÏòÉùÃ÷******************************/
/************************Ä£¿é±äÁ¿******************************/
/************************È«¾Ö±äÁ¿******************************/
/************************Êµ   ÏÖ*******************************/

/* @MODULE> */

/**
 * @brief:
 *    Ê¹µ±Ç°ÈÎÎñË¯ÃßÖ¸¶¨Ê±¼ä¡£
 * @param[in]: ticks: Ë¯ÃßÊ±¼ä
 * @return:
 *    TTOS_CALLED_FROM_ISR£ºÔÚÐéÄâÖÐ¶Ï´¦Àí³ÌÐòÖÐÖ´ÐÐ´Ë½Ó¿Ú¡£
 *    TTOS_INVALID_USER£ºµ±Ç°ÈÎÎñÎªIDLEÈÎÎñ¡£
 *    TTOS_INVALID_TIME£ºË¯ÃßÊ±¼äÎª0¡£
 *    TTOS_OK£ºÊ¹µ±Ç°ÈÎÎñË¯ÃßÖ¸¶¨Ê±¼ä³É¹¦¡£
 *    TTOS_SIGNAL_INTR:±»ÐÅºÅ»½ÐÑ¡£
 * @implements: RTE_DTASK.36.1
 */
T_TTOS_ReturnCode TTOS_SleepTask (T_UWORD ticks)
{
    TBSP_MSR_TYPE            msr  = 0U;
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask ();
    if (NULL == task)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET: ÔÚÐéÄâÖÐ¶Ï´¦Àí³ÌÐòÖÐÖ´ÐÐ´Ë½Ó¿Ú */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: µ±Ç°ÈÎÎñÊÇIDLEÈÎÎñ */
    if (TRUE == ttosIsIdleTask (task))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_USER */
        return (TTOS_INVALID_USER);
    }

    /* @REPLACE_BRACKET: <time>µÈÓÚÁã */
    if (0U == ticks)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_TIME */
        return (TTOS_INVALID_TIME);
    }

    /* @KEEP_COMMENT: ½ûÖ¹ÐéÄâÖÐ¶Ï */
    TBSP_GLOBALINT_DISABLE_WITH_LOCK (msr);

    /*
     *maoyz,2022-06-13:
     *若另外一个核上的task先调用TTOS_StopTask来停止当前的task,在停止过程中，当前task再调用TTOS_SleepTask,则可能出现如下错误情况:
     *1.TTOS_StopTask关中断，获取锁，进行后续操作
     *2.TTOS_SleepTask关中断，获取不到锁(由于步骤1)，因此自旋等待锁
     *3.步骤1中，后续操作会将被停止的task设置为TTOS_TASK_DORMANT状态，然后再释放锁
     *4.步骤2获取到锁后，还会继续执行正常路径，将自己加入到tick等待队列(错误情况，被停止的task不应该再继续运行)
     *5.在tick等待时间到后，将其唤醒，或上就绪态，此时task的状态将变为(TTOS_TASK_DORMANT|TTOS_TASK_READY)，继续运行(错误情况)
     *6.由于拥有TTOS_TASK_DORMANT状态的task在调度时，不会保存上下文，所以，该task未保存上下文就被切走,在恢复运行后，可能出现异常(错误情况)
     */
    /* 为解决上述错误，增加task状态检测，若task状态为TTOS_TASK_DORMANT表示其它核上的task将当前task停止过了
     */
    if (TTOS_TASK_DORMANT == task->state)
    {
        TBSP_GLOBALINT_ENABLE_WITH_LOCK (msr);
        return (TTOS_INVALID_STATE);
    }
    (void)ttosClearTaskReady (task);
    /* @KEEP_COMMENT: ÉèÖÃµ±Ç°ÔËÐÐÈÎÎñµÄµÈ´ýÊ±¼äÎª<ticks> */
    task->waitedTicks = ticks;
    /*
     * @KEEP_COMMENT: µ÷ÓÃttosInsertWaitedTask(DT.2.26)½«µ±Ç°ÔËÐÐÈÎÎñ
     * ttosManager.runningTask²åÈëÈÎÎñtickµÈ´ý¶ÓÁÐ
     */
    (void)ttosInsertWaitedTask (task);


    /* @KEEP_COMMENT: ÉèÖÃµ±Ç°ÔËÐÐÈÎÎñµÄ×´Ì¬ÎªTTOS_TASK_WAITING */
    task->state = (T_UWORD)TTOS_TASK_DELAYING | TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL;
    (void)ttosScheduleInIntDisAndKernelLock (msr);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (task->wait.returnCode);
}
