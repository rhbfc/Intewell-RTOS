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
#include <commonTypes.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************外部声明*****************************/
/************************前向声明*****************************/
/* @MODULE> */
/* 实现 */

/*
 * @brief:
 *    将消息插入到消息挂起队列上。
 * @param[in] theMessageQueue: 指定消息队列
 * @param[in] theMessage: 指定消息
 * @param[in] isUrgent: 消息是否紧急标志
 * @return:
 *    无
 * @implements: RTE_DMSGQ.9.1
 */
void ttosInsertMsg (T_TTOS_MsgqControlBlock *theMessageQueue,
                    T_TTOS_MsgqBufControl *theMessage, T_UWORD isUrgent)
{
    TBSP_MSR_TYPE msr = 0U;

    T_BOOL notify;

    /* @KEEP_COMMENT: 由于允许在中断中发送消息，因此需要关中断保障操作的完整性
     */
    TBSP_GLOBALINT_DISABLE (msr);

    /* 消息队列为空并且没有任务在receive的情况下才发出notify */
    notify = ((theMessageQueue->pendingCount == 0) && (ttosTaskqIsEmpty (&(theMessageQueue->receiveWaitQueue)) == TRUE));

    /* @KEEP_COMMENT: 挂起消息数加1，表示又有一个消息等待接收 */
    theMessageQueue->pendingCount++;

    /* @KEEP_COMMENT:
     * 紧急消息插入到消息挂起队列首部，非紧急消息插入到消息挂起队列的尾部 */
    /* @REPLACE_BRACKET: U(0) != isUrgent */
    if (0U != isUrgent)
    {
        /* @KEEP_COMMENT: 插入队列首部 */
        list_add_head (&theMessage->Node, &theMessageQueue->pendingMsgQueue[0]);
    }

    else
    {
        /* @KEEP_COMMENT: 设置消息的优先级在优先级位图中的相应位 */
        TTOS_MSGQ_SET_PRIORITYBITMAP (theMessageQueue->msgPriorityBitMap,
                                      theMessage->priority);
        /* @KEEP_COMMENT: 将消息插入对应优先级消息队列尾部 */
        list_add_tail (&theMessage->Node,
                       &theMessageQueue->pendingMsgQueue[theMessage->priority]);
    }

	if (notify && (theMessageQueue->notifyHandler != NULL))
	{
        (*theMessageQueue->notifyHandler)(theMessageQueue->notifyArgument);
	}

    /* @KEEP_COMMENT: 恢复中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}

/*
 * @brief:
 *    提交消息。
 * @param[in] theMessageQueue: 指定消息队列
 * @param[in] theMessage: 指定消息
 * @param[in] buffer: 要提交的消息缓冲
 * @param[in] size: 要提交的消息大小
 * @param[in] isUrgent: 消息是否紧急标志
 * @return:
 *    无
 * @implements: RTE_DMSGQ.9.2
 */
void ttosCommitMessage (T_TTOS_MsgqControlBlock *theMessageQueue,
                        T_TTOS_MsgqBufControl *theMessage, T_VOID *buffer,
                        T_UWORD size, T_UWORD isUrgent, TBSP_MSR_TYPE msr)
{
    T_TTOS_TaskControlBlock *theTask;
    T_UBYTE                  priority = 0;

    /* 执行到此处时，是禁止中断和禁止调度的。*/

    /*
     *以前的实现是先判断是否有任务在等消息，若没有，则将消息放入消息队列；若有，则直接将消息拷贝给等待任务。
     *由于将消息放入消息队列中的将消息拷贝到消息结构步骤是在开中断和调度下进行的，所以，会存在如下错误情况:
     *1.task1发送消息时，发现没有任务在等待消息，则将消息拷贝到消息结构中，在拷贝过程中，切换到task2
     *2.task2接收消息，发现此时消息队列中没有消息(由于task1将消息拷贝到消息结构中操作还未完成),则会加入到该消息队列的等待链表中；
     *3.由于task1拷贝完成后，不会再检测是否有任务在等待，所以，造成了消息队列中虽有消息，却有任务还在等消息的场景。
     *4.task1再次发送消息，发现有任务在等待消息，则将要发送的数据直接拷贝给等待任务。这就造成了fifo策略的破坏，理应先将之前已经存在消息
     *队列中的消息拷贝给等待任务。
     *
     *为解决该问题，先将消息加入消息队列,再判断是否有任务在等待消息，若有，则将消息从消息队列上取出。
     */

    /*
     *超时时，定时器中断处理程序中会访问此链表，
     *所以此处需要关中断访问。
     */

    /* 为了保证关中断时间的确定性，拷贝消息过程中是使能中断的*/
    TBSP_GLOBALINT_ENABLE (msr);
    ttosEnableTaskDispatchWithLock ();

    /* 把消息内容拷贝到消息结构中 */
    ttosCopyString (theMessage->buffer, buffer, size);

    TBSP_GLOBALINT_DISABLE (msr);
    ttosDisableTaskDispatchWithLock ();

    /* 填写消息实际大小 */
    theMessage->size = size;

    /* 将消息块插入到消息挂起队列上 */
    ttosInsertMsg (theMessageQueue, theMessage, isUrgent);

    /* 检查是否有等待接收消息的任务*/
    theTask = ttosDequeueTaskFromTaskq (&(theMessageQueue->receiveWaitQueue));
    if (NULL != theTask)
    {
        /* 有等待接收消息的任务，将消息从消息队列取出，拷贝给接收任务 */

        priority = zero_num_of_word_get (theMessageQueue->msgPriorityBitMap);

        theMessage = list_entry (list_first (&(theMessageQueue->pendingMsgQueue[priority])),
                                 T_TTOS_MsgqBufControl, Node);

        /* 从pendingMsgQueue消息队列中移除消息节点*/
        list_delete (&(theMessage->Node));

        /* 优化会导致指令重新排序，添加编译屏障保证顺序排列指令 */
        TTOS_COMPILER_MEMORY_BARRIER ();

        /* 处理消息位图 */
        if (TRUE
            == list_is_empty (&(theMessageQueue->pendingMsgQueue[priority])))
        {
            /* 清除消息的优先级在优先级位图中对应的位 */
            TTOS_MSGQ_CLEAR_PRIORITYBITMAP (theMessageQueue->msgPriorityBitMap,
                                            priority);
        }

        /* 挂起的消息数减1 */
        theMessageQueue->pendingCount--;

        /*
         *  此时，theTask不在任何链表中，仅当前任务才能将其唤醒。若当前任务为优先级极低的任务，theTask为tcpip_thread,当开调度后，若当前任务被抢占，直到当前任务
         *  恢复执行，并将theTask设置为就绪态，并开调度，theTask才能得到执行,网络才能通。在复杂场景下，由于当前任务优先级极低，可能存在当前任务一直不能得到执行的情况，
         *  导致theTask将一直不能被唤醒，网络一直不通。为解决tcpip_thread在被低优先级任务唤醒过程中，低优先级任务被抢占，导致tcpip_thread迟迟不能被唤醒，导致网络不通的问题，
         *  所以，在关调度下进行对theTask的唤醒。
         */
        TBSP_GLOBALINT_ENABLE (msr);

        ttosCopyString (theTask->wait.returnArgument, theMessage->buffer,
                        theMessage->size);

        TBSP_GLOBALINT_DISABLE (msr);

        ttosClearTaskWaiting (theTask);

        /* 记录实际接收到的消息大小 */
        *((T_UWORD *)(theTask->wait.returnArgument1)) = size;
        /* @KEEP_COMMENT: 设置消息优先级 */
        theTask->wait.count = theMessage->priority;
        /* @KEEP_COMMENT: 回收消息结构 */
        list_add_tail (&(theMessage->Node), &(theMessageQueue->inactiveMsgQueue));
    }

    theMessageQueue->disableDeleteFlag = FALSE;

    TBSP_GLOBALINT_ENABLE (msr);
    ttosEnableTaskDispatchWithLock ();
}

/*
* @brief
*     设置消息通知处理函数和参数。
* @param[in]  theMessageQueue: 指定消息队列
* @param[in]  handler: 消息通知处理函数
* @param[in]  argument: 消息通知参数
* @return
*       none
*/
void ttosSetMessageNotify(T_TTOS_MsgqControlBlock *theMessageQueue, MSGQ_NOTIFY_HANDLER handler, void *argument)
{
    theMessageQueue->notifyHandler  = handler;
    theMessageQueue->notifyArgument = argument;
}
