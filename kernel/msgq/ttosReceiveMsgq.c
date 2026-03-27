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

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/* @MODULE> */

/************************外部声明******************************/
T_EXTERN void ttosDisableTaskDispatchWithLock (void);
T_VOID osFaultHook (T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2,
                    T_WORD errorCode);

/************************前向声明******************************/

/* 实现 */

/*
 * @brief:
 *    接收消息。
 * @param[in] theMessageQueue: 指定消息队列
 * @param[in] theMessage: 指定消息
 * @param[out] buffer: 存放接收消息的缓冲
 * @param[out] size: 本次接收消息的大小
 * @return:
 *    无
 * @implements: RTE_DMSGQ.10.1
 */
T_MODULE void ttosReceiveMessage (T_TTOS_MsgqControlBlock *theMessageQueue,
                                  T_TTOS_MsgqBufControl   *theMessage,
                                  T_VOID *buffer, T_UWORD *size)
{
    T_TTOS_TaskControlBlock *theTask;
    TBSP_MSR_TYPE            msr = 0U;
    /* @KEEP_COMMENT: 用task接收ttosGetRunningTask函数返回值 */
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask ();
    /* @REPLACE_BRACKET: task 为空地址 */
    if (task == NULL)
    {
        return;
    }
    /*执行到此处时，中断使能开关是恢复了的*/
    /* @KEEP_COMMENT: 返回接收到的实际数据大小 */
    *size = theMessage->size;
    /* @KEEP_COMMENT: 将消息拷贝到接收任务的缓冲 */
    (void)ttosCopyString (buffer, theMessage->buffer, *size);
    (void)ttosDisableTaskDispatchWithLock ();
    TBSP_GLOBALINT_DISABLE (msr);
    /*
     *超时时，定时器中断处理程序中会访问此链表，
     *所以此处需要关中断访问。
     */
    /* @KEEP_COMMENT:
     * 检查消息队列的发送等待队列是否有等待任务,返回值赋值给theTask */
    theTask = ttosDequeueTaskFromTaskq (&(theMessageQueue->sendWaitQueue));

    /* @REPLACE_BRACKET: theTask != NULL */
    if (theTask != NULL)
    {
        /*
         *为了保证关中断时间的确定性，拷贝消息过程中需要恢复中断，中断处理程序中是不允许
         *删除、停止、重启任务的。
         */
        TBSP_GLOBALINT_ENABLE (msr);
        /*
         *在使能调度后，拷贝消息到接收任务的消息接收缓冲中的过程中，发送任务是不允许
         *被删除的，否则消息接收缓冲空间可能是冲突的。
         */

        /*
         *  此时，theTask不在任何链表中，仅当前任务才能将其唤醒。若当前任务为优先级极低的任务，theTask为tcpip_thread,当开调度后，若当前任务被抢占，直到当前任务
         *  恢复执行，并将theTask设置为就绪态，并开调度，theTask才能得到执行,网络才能通。在复杂场景下，由于当前任务优先级极低，
         *  可能存在当前任务一直不能得到执行的情况，导致theTask将一直不能被唤醒，网络一直不通。为解决tcpip_thread在被低优先级任务唤醒过程中，低优先级任务被抢占，
         *  导致tcpip_thread迟迟不能被唤醒，导致网络不通的问题，所以，在关调度下进行对theTask的唤醒。
         */
        // ttosEnableTaskDispatchWithLock();
        /* @KEEP_COMMENT: 把消息内容拷贝到消息结构中 */
        //        ttosCopyString(theMessage->buffer, (T_UWORD
        //        *)(theTask->wait.returnArgument),
        //        (T_UWORD)(theTask->wait.returnArgument1));
        (void)ttosCopyString (
            theMessage->buffer, theTask->wait.returnArgument,
            (T_UWORD)((T_ULONG)theTask->wait.returnArgument1));
        // ttosDisableTaskDispatchWithLock();
        TBSP_GLOBALINT_DISABLE (msr);

        ttosClearTaskWaiting (theTask);

        /* 记录发送消息的大小 */
        //        theMessage->size = (T_UWORD)(theTask->wait.returnArgument1);
        theMessage->size = (T_UWORD)((T_ULONG)(theTask->wait.returnArgument1));
        /* @KEEP_COMMENT: 设置消息优先级 */
        theMessage->priority = (T_UBYTE)theTask->wait.count;
        /* @KEEP_COMMENT: 将消息插入消息队列 */
        (void)ttosInsertMsg (theMessageQueue, theMessage, theTask->wait.option);
    }

    else
    {
        /* @KEEP_COMMENT: 消息队列没有等待的发送任务则将消息块放回空闲链中 */
        list_add_tail (&theMessage->Node, &theMessageQueue->inactiveMsgQueue);
    }

    /* @KEEP_COMMENT: 设置当前执行任务获取的消息的优先级*/
    task->wait.count                   = theMessage->priority;
    theMessageQueue->disableDeleteFlag = FALSE;

    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
}

/**
 * @brief:
 *    接收指定消息。
 * @param[in]  msgqID: 指定消息队列ID
 * @param[in]  options: 消息接收的属性设置
 * @param[in]  ticks: 设置等待的最大时间，单位为tick
 * @param[out] buffer: 指定存放接收到的消息的缓冲区地址
 * @param[out] size: 本次接收消息的大小
 * @return:
 *    TTOS_CALLED_FROM_ISR: 从中断中调用
 *    TTOS_INVALID_USER：当前任务为IDLE任务
 *    TTOS_INVALID_ADDRESS: buffer为NULL或size为NULL
 *    TTOS_INVALID_ID: 无效的消息队列标识符
 *    TTOS_UNSATISFIED:
 * 采用非等待方式时(options中包含TTOS_NO_WAIT或者ticks为0)当前没有挂起的消息
 *    TTOS_TIMEOUT: 接收消息超时
 *    TTOS_OBJECT_WAS_DELETED:  等待的消息队列已经被删除
 *    TTOS_OK: 成功接收指定消息
 * @implements:  RTE_DMSGQ.10.2
 */
T_TTOS_ReturnCode TTOS_ReceiveMsgq (MSGQ_ID msgqID, T_VOID *buffer,
                                    T_UWORD *size, T_UWORD options,
                                    T_UWORD ticks)
{
    T_TTOS_TaskControlBlock *executing;
    T_TTOS_MsgqControlBlock *theMessageQueue;
    T_TTOS_MsgqBufControl   *theMessage;
    struct list_node        *msgNode;
    TBSP_MSR_TYPE            msr = 0U;
    T_UWORD                  priority;
    /* @KEEP_COMMENT: 获取当前执行任务 */
    executing = ttosGetRunningTask ();

    /* 从中断中调用 */
    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: 当前任务是IDLE任务 */
    if (TRUE == ttosIsIdleTask (executing))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_USER */
        return (TTOS_INVALID_USER);
    }

    /* 接收缓冲区不能够是空；返回size指针也不能够为空 */
    /* @REPLACE_BRACKET: (NULL == buffer)  || (NULL == size) */
    if ((NULL == buffer) || (NULL == size))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 根据id获取消息队列对象，对象为空返回无效ID状态值 */
    theMessageQueue = (T_TTOS_MsgqControlBlock *)ttosGetObjectById (
        msgqID, TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET: theMessageQueue == NULL */
    if (theMessageQueue == NULL)
    {

        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* 由于允许在中断下发送消息，因此获取消息时需要关中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    /* 获取挂起的消息块 */
    /* @REPLACE_BRACKET: theMessageQueue->pendingCount != U(0) */
    if (theMessageQueue->pendingCount != 0U)
    {
        priority = zero_num_of_word_get (theMessageQueue->msgPriorityBitMap);
        /* @KEEP_COMMENT: 获取pendingMsgQueue首节点，返回值赋值给theMessage */
        msgNode = list_first (&(theMessageQueue->pendingMsgQueue[priority]));
        theMessage = (msgNode == NULL)
                         ? NULL
                         : list_entry (msgNode, T_TTOS_MsgqBufControl, Node);
        /* @REPLACE_BRACKET: theMessage为空地址 */
        if (theMessage == NULL)
        {
            /* @KEEP_COMMENT: 恢复中断 */
            TBSP_GLOBALINT_ENABLE (msr);


            /* @KEEP_COMMENT: 恢复调度 */
            (void)ttosEnableTaskDispatchWithLock ();

            /* @REPLACE_BRACKET: TTOS_INVALID_ID */
            return (TTOS_INVALID_ID);
        }
        /* @KEEP_COMMENT: 从pendingMsgQueue消息队列中移除消息节点 */
        list_delete (&(theMessage->Node));
        /* @KEEP_COMMENT: 优化会导致指令重新排序，添加编译屏障保证顺序排列指令
         */
        TTOS_COMPILER_MEMORY_BARRIER ();

        /* 处理消息位图 */
        /* @REPLACE_BRACKET: TRUE ==
         * list_is_empty(&(theMessageQueue->pendingMsgQueue[priority])) */
        if (TRUE
            == list_is_empty (&(theMessageQueue->pendingMsgQueue[priority])))
        {
            /* @KEEP_COMMENT: 清除消息的优先级在优先级位图中对应的位 */
            TTOS_MSGQ_CLEAR_PRIORITYBITMAP (theMessageQueue->msgPriorityBitMap,
                                            priority);
        }

        /* @KEEP_COMMENT: 挂起的消息数减1 */
        theMessageQueue->pendingCount--;
        /* @KEEP_COMMENT: 为了保证关中断时间的确定性，此处需要恢复中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        /*
         *消息节点已经从消息队列取下，此时消息队列和当前运行任务是不允许被删除的，
         *否则消息节点就不能得到回收。
         */
        theMessageQueue->disableDeleteFlag = TRUE;


        /* @KEEP_COMMENT: 恢复调度 */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @KEEP_COMMENT: 接收处理该消息 */
        ttosReceiveMessage (theMessageQueue, theMessage, buffer, size);
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* 没有挂起的消息时，如果不要求等待，则直接返回 */
    /* @REPLACE_BRACKET: ((options & TTOS_NO_WAIT) != U(0)) ||(U(0) == ticks) */
    if (((options & TTOS_NO_WAIT) != 0U) || (0U == ticks))
    {
        /* @KEEP_COMMENT: 恢复中断 */
        TBSP_GLOBALINT_ENABLE (msr);


        /* @KEEP_COMMENT: 恢复调度 */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @KEEP_COMMENT: 等待对象信息 */
    executing->wait.id         = (T_VOID *)msgqID;
    executing->wait.returnCode = TTOS_OK;
    /* @KEEP_COMMENT: 接收参数信息 */
    executing->wait.returnArgument  = buffer;
    executing->wait.returnArgument1 = ((T_VOID *)size);

    if (!pcb_signal_pending ((pcb_t)executing->ppcb))
    {
        /* @KEEP_COMMENT: 将当前任务插入到消息的接收等待队列中 */
        (void)ttosEnqueueTaskq (&(theMessageQueue->receiveWaitQueue), ticks, TRUE);
    }

    if (1 != ttosGetDisableScheduleLevelWithCpuID(cpuid_get()))
    {
        (void)osFaultHook (TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__,
                           __LINE__, TTOS_TASK_ENTRY_RETURN_ERROR);
    }

    /* @KEEP_COMMENT: 恢复中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT: 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* 返回任务醒来的返回值 */
    /* @REPLACE_BRACKET: executing->wait.returnCode */
    return (executing->wait.returnCode);
}
