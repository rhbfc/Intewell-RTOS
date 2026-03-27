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
#ifndef TTOSUTILS_INL
#define TTOSUTILS_INL

/* @<MOD_HEAD */
#include <atomic.h>
#include <linux/wait.h>
#include <ttosBase.h>
#include <ttosHal.h>
#include <ttosHandle.h>
#include <ttosTypes.h>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************外部声明******************************/

/* @<MOD_EXTERN */
/* ttos控制结构 */
T_EXTERN T_TTOS_FREE_STACK_ROUTINE ttosFreeStackRoutine;
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
/* @MOD_EXTERN> */
/************************前向声明******************************/

/************************模块变量******************************/

/************************实    现******************************/
#ifdef __cplusplus
extern "C" {
#endif

/* @MODULE> */

/*
 * @brief:
 *    初始化TTOS对象节点。
 * @param[in]: objectID: 对象的ID号
 * @param[in]: objectType: 对象类型
 * @return:
 *    无
 * @tracedREQ:
 * @implements:
 */
void ttosInitObjectCore(T_VOID *objectID, T_TTOS_ObjectType objectType)
{
    T_TTOS_ObjectCore *objectCore = (T_TTOS_ObjectCore *)objectID;
    objectCore->handle.magic = (T_ULONG)objectID;
    objectCore->handle.type = (T_ULONG)objectType;
    /* @KEEP_COMMENT: 对象资源节点记录对应的对象*/
    objectCore->objResourceNode.objResourceObject = (struct list_node *)&(objectCore->objectNode);

    /* @KEEP_COMMENT: 将对象添加到正在使用的资源队列中*/
    list_add_tail(&(objectCore->objResourceNode.objectResourceNode),
                  &(ttosManager.inUsedResource[objectType]));

    ttosManager.inUsedResourceNum[objectType]++;
    atomic_set(&objectCore->refcnt, 1);
    init_waitqueue_head(&objectCore->wq);
}

void ttosObjectWaitAndSetToFree(T_TTOS_ObjectCoreID objectCore, T_BOOL isInterruptible)
{
    long ref = 1;
    wait_event_with_flag(&objectCore->wq, atomic_cas(&objectCore->refcnt, ref, 0), isInterruptible);
}

T_BOOL ttosObjectGet(T_TTOS_ObjectCoreID objectCore)
{
    long ref = atomic_read(&objectCore->refcnt);

    if (ref == 0)
    {
        return FALSE;
    }

    while (!atomic_cas(&objectCore->refcnt, ref, ref + 1))
    {
        ref = atomic_read(&objectCore->refcnt);
        if (ref == 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

T_BOOL ttosObjectPut(T_TTOS_ObjectCoreID objectCore)
{
    long ref = atomic_read(&objectCore->refcnt);
    if (unlikely(ref <= 1))
    {
        return FALSE;
    }
    while (unlikely(!atomic_cas(&objectCore->refcnt, ref, ref - 1)))
    {
        ref = atomic_read(&objectCore->refcnt);
        if (unlikely(ref <= 1))
        {
            return FALSE;
        }
    }

    if ((ref - 1) == 1)
    {
        wake_up(&objectCore->wq);
    }

    return TRUE;
}

/*
 * @brief:
 *    根据对象ID，获取TTOS对象节点。
 * @param[in]: objectID: 对象的ID号
 * @param[in]: objectType: 对象类型
 * @return:
 *    NULL: 对象不存在。
 *    TTOS对象节点。
 * @notes:
 *    由于任务上下文中存在删除对象的情况，所以获取对象后，
 *    此接口是禁止了任务调度的。
 * @tracedREQ: RT
 * @implements: DT.9.12
 */
T_TTOS_ObjectCoreID ttosGetObjectById(T_VOID *objectID, T_TTOS_ObjectType objectType)
{
    T_TTOS_ObjectCoreID objectCoreID;
    T_UWORD ret;

    /* @REPLACE_BRACKET: TTOS_OBJECT_TASK == objectType */
    if (TTOS_OBJECT_TASK == objectType)
    {
        /* @REPLACE_BRACKET: TTOS_SELF_OBJECT_ID == objectID */
        if (TTOS_SELF_OBJECT_ID == objectID)
        {
            T_TTOS_TaskControlBlock *task = ttosGetRunningTask();

            /* @KEEP_COMMENT: 当前任务是IDLE任务 */
            /* @REPLACE_BRACKET: TRUE == ttosIsIdleTask(task)*/
            if (TRUE == ttosIsIdleTask(task))
            {
                /* @KEEP_COMMENT: 不能操作IDLE任务。 */
                objectCoreID = NULL;
            }

            else
            {
                (void)ttosDisableTaskDispatchWithLock();
                objectCoreID = &(task->objCore);
            }

            /* @REPLACE_BRACKET: objectCoreID*/
            return (objectCoreID);
        }
    }

    ret = ttosVerifyObjectCore((T_TTOS_ObjectCoreID)objectID, (T_UWORD)objectType);

    /* @REPLACE_BRACKET: TTOS_VERIFY_OBJCORE_OK == ret*/
    if (TTOS_VERIFY_OBJCORE_OK == ret)
    {
        /* IDLE任务没有初始化对象管理结构，如果是其它CPU上的IDLE任务，也会走此分支。*/
        /* @KEEP_COMMENT:
         * 由于任务上下文中存在删除对象的情况，所以获取对象后，此接口是禁止了任务调度的。*/
        (void)ttosDisableTaskDispatchWithLock();
        objectCoreID = (T_TTOS_ObjectCoreID)objectID;
    }

    else
    {
        objectCoreID = NULL;
    }

    /* @REPLACE_BRACKET: objectCoreID*/
    return (objectCoreID);
}

/*
 * @brief:
 *    检查对象ID类型是否与指定的对象类型一致。
 * @param[in]: objectID: 对象的ID号
 * @param[in]: objectType: 对象类型
 * @return:
 *    FALSE: 对象ID类型是与指定的对象类型不一致。
 *    TRUE: 对象ID类型是与指定的对象类型一致。
 * @implements: RTE_DUTILS.9.3
 */
T_BOOL ttosObjectTypeIsOk(T_VOID *objectID, T_TTOS_ObjectType objectType)
{
    T_UWORD ret;

    /* @REPLACE_BRACKET: (TTOS_OBJECT_TASK == objectType) &&
     * (TTOS_SELF_OBJECT_ID == objectID)*/
    if ((TTOS_OBJECT_TASK == objectType) && (TTOS_SELF_OBJECT_ID == objectID))
    {
        /* @REPLACE_BRACKET: TRUE*/
        return (TRUE);
    }

    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 判断对象的handle是否是有效的 */
    ret = ttosVerifyObjectCore((T_TTOS_ObjectCoreID)objectID, (T_UWORD)objectType);
    (void)ttosEnableTaskDispatchWithLock();

    /* @REPLACE_BRACKET: TTOS_VERIFY_OBJCORE_TYPE_FAIL == ret*/
    if (TTOS_VERIFY_OBJCORE_TYPE_FAIL == ret)
    {
        /* @REPLACE_BRACKET: FALSE*/
        return (FALSE);
    }

    else
    {
        /* @REPLACE_BRACKET: TRUE*/
        return (TRUE);
    }
}

/*
 * @brief:
 *    根据对象名字，获取TTOS对象节点。
 * @param[in]: objectID: 对象的ID号
 * @param[in]: objectType: 对象类型
 * @return:
 *    NULL: 对象不存在。
 *    TTOS对象节点。
 * @implements: RTE_DUTILS.9.4
 */
T_TTOS_ObjectCoreID ttosGetIdByName(T_UBYTE *objectCoreName, T_TTOS_ObjectType objectType)
{
    T_TTOS_ObjectCore *objectCoreID;
    struct list_node *pNode;
    T_TTOS_ObjectCoreID returnObjectCoreID = NULL;
    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT:  获取链表的第一个节点*/
    pNode = list_first(&(ttosManager.inUsedResource[objectType]));

    /* @REPLACE_BRACKET: pNode != NULL*/
    while (pNode != NULL)
    {
        objectCoreID = (T_TTOS_ObjectCoreID)(((T_TTOS_ObjResourceNode *)pNode)->objResourceObject);

        /* @REPLACE_BRACKET: TRUE ==
         * ttosCompareName(objectCoreName,objectCoreID->objName)*/
        if (TRUE == ttosCompareName(objectCoreName, objectCoreID->objName))
        {
            returnObjectCoreID = objectCoreID;
            break;
        }

        /* @KEEP_COMMENT:  获取节点<node>的下一个节点*/
        pNode = list_next(pNode, &(ttosManager.inUsedResource[objectType]));
    }

    (void)ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: returnObjectCoreID*/
    return (returnObjectCoreID);
}

/*
 * @brief:
 *    根据对象名字，获取TTOS对象节点。
 * @param[in]: idArea: 存放对象标识符的缓冲
 * @param[in]: areaSize: 缓冲区最大存放数目
 * @param[in]: objectType: 对象类型
 * @return:
 *  已存放的 对象个数。
 * @implements: RTE_DUTILS.9.5
 */
T_UWORD ttosGetIdList(T_TTOS_ObjectCoreID *ttosArea, UINT32 areaSize, T_TTOS_ObjectType objectType)
{
    T_UWORD listAmount = 0U;
    T_TTOS_ObjectCore *objectCoreID;
    struct list_node *pNode;
    (void)ttosDisableTaskDispatchWithLock();

    /* @KEEP_COMMENT:  获取链表的第一个节点*/
    pNode = list_first(&(ttosManager.inUsedResource[objectType]));

    /* @REPLACE_BRACKET: pNode != NULL*/
    while (pNode != NULL)
    {
        /* @REPLACE_BRACKET: listAmount == areaSize*/
        if (listAmount == areaSize)
        {
            break;
        }

        objectCoreID = (T_TTOS_ObjectCoreID)(((T_TTOS_ObjResourceNode *)pNode)->objResourceObject);
        *ttosArea = objectCoreID;
        ttosArea++;
        listAmount++;
        /* @KEEP_COMMENT:  获取节点<node>的下一个节点*/
        pNode = list_next(pNode, &(ttosManager.inUsedResource[objectType]));
    }

    (void)ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: listAmount*/
    return (listAmount);
}

/*
 * @brief:
 *    根据对象类型，获取TTOS对象节点。
 * @param[in]: objectType: 对象类型
 * @return:
 *    NULL: 对象不存在。
 *    TTOS对象节点。
 * @tracedREQ:
 * @implements:
 */
T_TTOS_ObjectCoreID ttosGetObjectFromInactiveResource(T_TTOS_ObjectType objectType)
{
    struct list_node *pNode;
    T_TTOS_ObjectCoreID returnObjectCoreID = NULL;
    pNode = list_first(&(ttosManager.inactiveResource[objectType]));

    /* @REPLACE_BRACKET: pNode == NULL*/
    if (pNode == NULL)
    {
        /* @REPLACE_BRACKET: NULL*/
        return (NULL);
    }

    /* @KEEP_COMMENT: 从空闲对象资源队列中移除对象控制块节点 */
    list_delete(pNode);
    returnObjectCoreID =
        (T_TTOS_ObjectCoreID)(((T_TTOS_ObjResourceNode *)pNode)->objResourceObject);
    /* 获取对象控制块 */
    /* @REPLACE_BRACKET: returnObjectCoreID*/
    return (returnObjectCoreID);
}

/*
 * @brief:
 *    根据对象类型，分配TTOS对象。
 * @param[in]: objectType: 对象类型
 * @param[out]: objectID: 对象ID
 * @return:
 *       TTOS_TOO_MANY:
 * 系统中正在使用的对象数已达到用户配置的最大对象数(静态配置的对象数+
 * 可创建的对象数)。 TTOS_UNSATISFIED: 分配任务对象失败。 TTOS_INVALID_TYPE:
 * 传入的对象类型不合法。 TTOS_OK: 成功分配对象。
 * @tracedREQ:
 * @implements:
 */
T_TTOS_ReturnCode ttosAllocateObject(T_TTOS_ObjectType objectType, T_VOID **objectID)
{
    T_UWORD size;
    /* @KEEP_COMMENT: 从空闲对象资源 队列中获取对象控制块*/
    *objectID = (T_VOID *)(ttosGetObjectFromInactiveResource(objectType));

    switch (objectType)
    {
    case TTOS_OBJECT_TASK:
        size = sizeof(T_TTOS_TaskControlBlock);
        break;

    case TTOS_OBJECT_SEMA:
        size = sizeof(T_TTOS_SemaControlBlock);
        break;

    case TTOS_OBJECT_TIMER:
        size = sizeof(T_TTOS_TimerControlBlock);
        break;

    case TTOS_OBJECT_MSGQ:
        size = sizeof(T_TTOS_MsgqControlBlock);
        break;

    default:
        size = 0U;
        break;
    }

    /* @REPLACE_BRACKET: NULL == *objectID*/
    if (NULL == *objectID)
    {
        /* @KEEP_COMMENT:
         * 判断系统中目前已启用的任务个数是否超过系统允许的最大任务个数 */
        /* @REPLACE_BRACKET: ttosManager.objExistNumber[objectType]==
         * ttosManager.objMaxNumber[objectType]*/
        if (ttosManager.objExistNumber[objectType] == ttosManager.objMaxNumber[objectType])
        {
            /* @REPLACE_BRACKET: TTOS_TOO_MANY*/
            return (TTOS_TOO_MANY);
        }

        /* @REPLACE_BRACKET: U(0) == size*/
        if (0U == size)
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_TYPE*/
            return (TTOS_INVALID_TYPE);
        }

        *objectID = memalign(TTOS_OBJECT_MALLOC_DEFAULT_SIZE, size);

        /* @REPLACE_BRACKET: NULL == *objectID*/
        if (NULL == *objectID)
        {
            /* @REPLACE_BRACKET: TTOS_UNSATISFIED*/
            return (TTOS_UNSATISFIED);
        }

        ttosManager.objExistNumber[objectType]++;
    }

    memset(*objectID, 0, size);
    /* @REPLACE_BRACKET: TTOS_OK*/
    return (TTOS_OK);
}

/*
 * @brief:
 *    根据对象类型，回收对象节点。
 * @param[in]: objectID: 对象的ID号
 * @param[in]: objectType: 对象类型
 * @param[in]: isExtracted: 指定的对象是否从对象资源队列中删除
 * @return:
 *    无
 * @tracedREQ:
 * @implements:
 */
void ttosReturnObjectToInactiveResource(T_VOID *objectID, T_TTOS_ObjectType objectType,
                                        T_BOOL isExtracted)
{
    T_TTOS_ObjectCore *objectCore;
    T_UWORD size;
    T_TTOS_TaskControlBlock *taskCB;
    objectCore = (T_TTOS_ObjectCore *)objectID;

    /* @REPLACE_BRACKET: FALSE == isExtracted*/
    if (FALSE == isExtracted)
    {
        /* @KEEP_COMMENT: 将对象控制块从系统正在使用的对象资源队列中删除*/
        list_delete(&(objectCore->objResourceNode.objectResourceNode));
    }

    /* @REPLACE_BRACKET: 虚拟中断类型 */
    switch (objectType)
    {
    case TTOS_OBJECT_TASK:
        taskCB = (T_TTOS_TaskControlBlock *)objectID;

        if (TRUE == taskCB->isFreeStack)
        {
            /*
             *运行任务退出时，进行任务切换时还会使用栈空间，在运行任务彻底不使用任务栈空间时
             *再释放。
             */
            if (ttosFreeStackRoutine != NULL)
            {
                /* @KEEP_COMMENT:
                 * 运行到此处时已经关调度，需使能调度，否则释放内存无法获取信号量时，任务无法被切换，继续运行
                 */
                (void)ttosEnableTaskDispatchWithLock();
                (*ttosFreeStackRoutine)(taskCB->stackStart);
                tid_obj_free(taskCB->tid);
                (void)ttosDisableTaskDispatchWithLock();
            }
        }

        size = sizeof(T_TTOS_TaskControlBlock);
        break;

    case TTOS_OBJECT_SEMA:
        size = sizeof(T_TTOS_SemaControlBlock);
        break;

    case TTOS_OBJECT_TIMER:
        size = sizeof(T_TTOS_TimerControlBlock);
        break;

    case TTOS_OBJECT_MSGQ:
        size = sizeof(T_TTOS_MsgqControlBlock);
        break;

    default:
        size = 0U;
        break;
    }

    /* @KEEP_COMMENT: 清除对象控制块中的数据*/
    (void)memAlignClear(objectCore, size, ALIGNED_BY_ONE_BYTE);
    /* @KEEP_COMMENT:
     * 对象资源节点记录对应的对象，这样ttosGetObjectFromInactiveResource()才能返回objResourceObject中记录的对象控制块。*/
    objectCore->objResourceNode.objResourceObject = (struct list_node *)&(objectCore->objectNode);
    /* @KEEP_COMMENT: 将对象控制块归还到系统空闲对象资源队列中*/
    list_add_tail(&(objectCore->objResourceNode.objectResourceNode),
                  &(ttosManager.inactiveResource[objectType]));
    ttosManager.inUsedResourceNum[objectType]--;
}

/*
 * @brief:
 *    使TTOS对象节点无效。
 * @param[in]: objectID: 对象的ID号
 * @return:
 *    无
 * @implements: RTE_DUTILS.9.9
 */
void ttosInvalidObjectCore(T_TTOS_ObjectCore *objectCore)
{
    /* @KEEP_COMMENT:
     * 设置<objectCore->handle.magic>与<objectCore->handle.type>值设置为0 */
    objectCore->handle.magic = 0UL;
    objectCore->handle.type = 0UL;
}

/*
 * @outDir: RTE_DMEM
 * @brief
 *       将src开始的内存空间的内容拷贝至dest指向的内存空间，
 *       拷贝的大小为size。
 * @param[in]  dest: 拷贝的目的地址
 * @param[in]  src: 拷贝的源地址
 * @param[in]  size: 拷贝大小，以字节为单位
 * @return
 *       none
 * @implements: RTE_DMEM.12
 */
void ttosCopyString(T_UWORD *dest, T_UWORD *src, T_UWORD size)
{
    T_UBYTE *tDest;
    T_UBYTE *tSrc;

    /* @KEEP_COMMENT: 如果源地址、目标地址均按4字节对齐，则按字拷贝*/
    /* @REPLACE_BRACKET: ((((T_ULONG)dest) % 4) == UL(0)) &&
     * ((((T_ULONG)src) % 4) == UL(0))*/
    if (((((T_ULONG)dest) % 4U) == 0UL) && ((((T_ULONG)src) % 4U) == 0UL))
    {
        /* @KEEP_COMMENT: 按4字节对齐部分按字拷贝 */
        /* @REPLACE_BRACKET: size >= U(4)*/
        while (size >= 4U)
        {
            *dest = *src;
            dest++;
            src++;
            size = size - 4;
        }
    }

    /* @KEEP_COMMENT:
     * 源地址或目标地址未按4字节对齐或按4字节对齐拷贝后仍有未拷贝数据，则直接按字节拷贝
     */
    tDest = (T_UBYTE *)dest;
    tSrc = (T_UBYTE *)src;

    /* @REPLACE_BRACKET: size != U(0)*/
    while (size != 0U)
    {
        *tDest = *tSrc;
        tDest++;
        tSrc++;
        size = size - 1;
    }
}

#ifdef __cplusplus
}
#endif

#endif /*_TTOSUTILS_INL*/
