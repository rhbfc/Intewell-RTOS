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

/* @MOD_HEAD> */
#include <ttosBase.h>
#include <ttosHandle.h>
#include <ttosInterTask.inl>
/* @MOD_HEAD> */
/************************Íâ²¿ÉùÃ÷******************************/
T_EXTERN void ttosDisableTaskDispatchWithLock (void);
/************************Ç°ÏòÉùÃ÷******************************/

/* @MODULE> */
/*ÊµÏÖ*/
/*
 * @brief
 *       ³õÊ¼»¯ÈÎÎñ¶ÓÁÐ¡£
 * @param      theTaskQueue: ÈÎÎñ¶ÓÁÐ¿ØÖÆ¿é
 * @param[in]  theDiscipline: ÈÎÎñ¶ÓÁÐµÄÀàÐÍ
 * @param[in]  state: ´¦ÓÚÈÎÎñ¶ÓÁÐÊ±ÈÎÎñµÄ×´Ì¬
 * @return
 *       none
 * @implements: RTE_DTASK.39.1
 */
void ttosInitializeTaskq (T_TTOS_Task_Queue_Control    *theTaskQueue,
                          T_TTOS_Task_Queue_Disciplines theDiscipline,
                          T_TTOS_TaskState              state)
{
    UINT32 idx;
    /* @KEEP_COMMENT:  ÉèÖÃÈÎÎñ¶ÓÁÐÊôÐÔ*/
    theTaskQueue->state      = (T_TTOS_TaskState)state;
    theTaskQueue->discipline = theDiscipline;

    /* @REPLACE_BRACKET:  ¸ù¾ÝÈÎÎñ¶ÓÁÐµÄ²»Í¬ÀàÐÍÇå¿ÕÈÎÎñ¶ÓÁÐÁ´±í*/
    if (theDiscipline == T_TTOS_QUEUE_DISCIPLINE_FIFO)
    {
        /* @KEEP_COMMENT:³õÊ¼»¯fifoµÈ´ý¶ÓÁÐ */
        INIT_LIST_HEAD(&theTaskQueue->queues.fifoQueue);
    }

    else
    {
        /* @REPLACE_BRACKET: idx=U(0) ; idx <
         * TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS ; idx++ */
        for (idx = 0U; idx < TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS; idx++)
        {
            /* @KEEP_COMMENT:³õÊ¼»¯ÓÅÏÈ¼¶µÈ´ý¶ÓÁÐ */
            INIT_LIST_HEAD(&theTaskQueue->queues.priorityQueue[idx]);
        }
    }
}

/*
 * @brief:
 *    ÅÐ¶ÏÈÎÎñ¶ÓÁÐÊÇ·ñÎª¿Õ¡£
 * @param[in]: theTaskQueue: ÈÎÎñ¶ÓÁÐ¿ØÖÆ¿é
 * @return:
 *    FALSE: Á´±í·Ç¿Õ¡£
 *    TRUE: Á´±íÎª¿Õ¡£
 * @implements: RTE_DTASK.39.2
 */
T_BOOL ttosTaskqIsEmpty (T_TTOS_Task_Queue_Control *theTaskQueue)
{
    UINT32 idx;
    T_BOOL ret = TRUE;

    /* @REPLACE_BRACKET: ¸ù¾ÝÈÎÎñ¶ÓÁÐµÄ²»Í¬ÀàÐÍÅÐ¶ÏÈÎÎñ¶ÓÁÐÁ´±í*/
    if (T_TTOS_QUEUE_DISCIPLINE_FIFO == theTaskQueue->discipline)
    {
        /* @REPLACE_BRACKET: ÅÐ¶ÏfifoµÈ´ý¶ÓÁÐ */
        if (TRUE != list_is_empty(&theTaskQueue->queues.fifoQueue))
        {
            ret = FALSE;
        }
    }

    else
    {
        /* @REPLACE_BRACKET: idx=U(0) ; idx <
         * TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS ; idx++ */
        for (idx = 0U; idx < TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS; idx++)
        {
            /* @REPLACE_BRACKET: ÅÐ¶ÏÓÅÏÈ¼¶µÈ´ý¶ÓÁÐ */
            if (TRUE
                != list_is_empty(&theTaskQueue->queues.priorityQueue[idx]))
            {
                ret = FALSE;
                break;
            }
        }
    }

    /* @REPLACE_BRACKET: ret */
    return (ret);
}

/*
 * @brief
 *       ÒÆ³öÖ¸¶¨µÄÈÎÎñ¶ÓÁÐÖÐËùÓÐµÄÈÎÎñ¡£
 * @param      theTaskQueue: ´ýÒÆ³öµÄÈÎÎñ¶ÓÁÐ
 * @param[in]  status: ÈÎÎñ·µ»ØÖµ
 * @return
 *       none
 * @implements: RTE_DTASK.39.3
 */
void ttosFlushTaskq (T_TTOS_Task_Queue_Control *theTaskQueue, UINT32 status)
{
    T_TTOS_TaskControlBlock *theTask;
    (void)ttosDisableTaskDispatchWithLock ();
    /* @KEEP_COMMENT: ÒÆ³öÈÎÎñ¶ÓÁÐÖÐÃ¿¸öÈÎÎñ*/
    theTask = ttosDequeueTaskq (theTaskQueue);

    /* @REPLACE_BRACKET: theTask != NULL */
    while (theTask != NULL)
    {
        theTask->wait.returnCode = (T_TTOS_ReturnCode)status;
        theTask->wait.id         = TTOS_NULL_OBJECT_ID;
        /* @KEEP_COMMENT:  ´Ë´¦Ê¹ÄÜÈÎÎñµ÷¶ÈÊÇÎªÁË±ÜÃâ½ûÖ¹µ÷¶ÈµÄÊ±¼ä¹ý³¤¡£*/
        (void)ttosEnableTaskDispatchWithLock ();
        (void)ttosDisableTaskDispatchWithLock ();
        theTask = ttosDequeueTaskq (theTaskQueue);
    }

    (void)ttosEnableTaskDispatchWithLock ();
}
