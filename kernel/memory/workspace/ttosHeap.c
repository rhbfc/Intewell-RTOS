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

/*
 *   n = sizeof(long)
 *            +--------------------------------+
 *     0      |  size = 0      | status = used |  a.k.a.  dummy back flag
 *            +--------------------------------+
 *     n      |  size = size-2n| status = free |  a.k.a.  front flag
 *            +--------------------------------+
 *    2n      |  next     = PERM HEAP_TAIL     |
 *            +--------------------------------+
 *    3n      |  previous = PERM HEAP_HEAD     |
 *            +--------------------------------+
 *            |                                |
 *            |      memory available          |
 *            |       for allocation           |
 *            |                                |
 *            +--------------------------------+
 *  size - 2n |  size = size-2n| status = free |  a.k.a.  back flag
 *            +--------------------------------+
 *  size - n  |  size = 0      | status = used |  a.k.a.  dummy front flag
 *            +--------------------------------+
 *
 */
/* @<MODULE */

/* 头文件 */
/* @<MOD_HEAD */
#include "ttosHeap.h"
#include <ttosTypes.h>
/* @MOD_HEAD> */

/* 宏定义 */
#define N                    sizeof (T_ULONG)
#define HEAP_BLOCK_USED      (1UL)       /* 内存块正在被使用标志 */
#define HEAP_BLOCK_FREE      ((UINT32)0) /* 内存块空闲标志 */
#define HEAP_DUMMY_FLAG      (0UL + HEAP_BLOCK_USED) /* 没有后续内存块标志 */
#define HEAP_TAIL_BLOCK_SIZE ((UINT32)2 * N) /* 堆的尾部内存块的控制块大小 */
#define HEAP_FREE_BLOCK_OVERHEAD                                               \
    ((UINT32)sizeof (T_TTOS_HeapBlock)) /* 空闲内存块的控制块大小 */
#define HEAP_USED_BLOCK_OVERHEAD                                               \
    (2UL * sizeof (T_ULONG)) /* 使用中的内存块控制块大小 */
#define HEAP_MINIMUM_HEAPSIZE                                                           \
    (HEAP_TAIL_BLOCK_SIZE + HEAP_FREE_BLOCK_OVERHEAD) /* 构建堆的最小的堆空间 \
                                                       */
#define HEAP_MINIMUM_BLOCKSIZE   (4UL * N) /* 最小内存块的大小  */
#define HEAP_MINIMUM_ALIGNEDSIZE ((UINT32)N)
#define HEAP_NEXTBLOCK(_block)                                                 \
    ((T_TTOS_HeapBlock *)(((UINT8 *)(_block))                                  \
                          + ((_block)->fflag                                   \
                             & (~HEAP_BLOCK_USED)))) /* 定位前向相邻内存块 */
#define HEAP_BLOCKAT(_block, _offset)                                          \
    ((T_TTOS_HeapBlock *)(((UINT8 *)(_block))                                  \
                          + (_offset))) /* 根据偏移定位内存块位置 */

/************************前向声明******************************/
/* @MODULE> */
/* 实现 */
/*
 * @brief
 *       初始化堆管理。
 * @param      hcb: 堆控制结构
 * @param[in]  startingAddress: 堆起始地址
 * @param[in]  heapSize: 堆大小
 * @return
 *       TTOS_INVALID_SIZE: 无效大小
 *       TTOS_OK: 初始化成功
 *       TTOS_INVALID_ADDRESS: hcb或startingAddress为NULL
 * @implements  DM.8.6,DC.26.5
 */
T_TTOS_ReturnCode ttosInitHeap (T_TTOS_HeapControl *hcb,
                                T_VOID *startingAddress, UINT32 heapSize)
{
    T_TTOS_HeapBlock *headBlock;
    T_TTOS_HeapBlock *tailBlock;
    UINT32            headBlockSize;

    /* @REPLACE_BRACKET: (hcb == NULL) || (startingAddress == NULL) */
    if ((hcb == NULL) || (startingAddress == NULL))
    {
        /* @KEEP_COMMENT: return TTOS_INVALID_ADDRESS */
        return TTOS_INVALID_ADDRESS;
    }

    /* @REPLACE_BRACKET: heapSize < HEAP_MINIMUM_HEAPSIZE */
    if (heapSize < HEAP_MINIMUM_HEAPSIZE)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* 构建头Block */
    /* @KEEP_COMMENT: 构建头Block */
    headBlockSize    = heapSize - HEAP_TAIL_BLOCK_SIZE;
    headBlock        = (T_TTOS_HeapBlock *)startingAddress;
    headBlock->bflag = HEAP_DUMMY_FLAG;
    headBlock->fflag = headBlockSize;
    headBlock->next
        = (T_TTOS_HeapBlock *)&hcb
              ->final; /* 头Block是首个空闲块，因此需要加入空闲链表中 */
    headBlock->previous = (T_TTOS_HeapBlock *)&hcb->start;
    /* 构建尾Block */
    /* @KEEP_COMMENT: 构建尾Block */
    tailBlock = HEAP_NEXTBLOCK (headBlock);

    /* @REPLACE_BRACKET: (tailBlock == NULL) */
    if (tailBlock == NULL)
    {
        /* @KEEP_COMMENT: return TTOS_INVALID_ADDRESS */
        return TTOS_INVALID_ADDRESS;
    }
    tailBlock->bflag = headBlockSize;
    tailBlock->fflag = HEAP_DUMMY_FLAG;
    /* @KEEP_COMMENT: 初始化堆管理结构 */
    hcb->heap_size       = heapSize;
    hcb->in_use_size     = 0U;
    hcb->free_blocks     = 1U;
    hcb->allocate_goods  = 0U;
    hcb->free_goods      = 0U;
    hcb->allocate_fails  = 0U;
    hcb->free_fails      = 0U;
    hcb->internal_errors = 0U;
    hcb->start           = headBlock;
    hcb->final           = tailBlock;
    hcb->first           = headBlock;
    hcb->null            = NULL;
    hcb->last            = headBlock;
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/*
 * @brief
 *       从指定的堆中分配指定大小的内存块。
 * @param      hcb: 被请求分配内存块的堆的控制结构
 * @param[in]  size: 请求分配的内存块的大小
 * @param[in]  alignedSize: 对齐大小
 * @param[in]  space: 所分配内存块的用户可使用空间的起始地址
 * @return
 *       TTOS_INVALID_STATE: 状态不正确
 *       TTOS_INVALID_ADDRESS: 输入参数hcb 或space 为空
 *       TTOS_INVALID_SIZE:
 * 输入参数size，或alignedSize为0，或计算实际分配空间大小超出0xFFFFFFFF
 *       TTOS_UNSATISFIED: 不能满足请求
 *       TTOS_INTERNAL_ERROR: 发生内部错误
 *       TTOS_OK: 分配成功
 * @implements  DM.8.8,DC.26.7
 */
T_TTOS_ReturnCode ttosAllocateHeap (T_TTOS_HeapControl *hcb, UINT32 size,
                                    UINT32 alignedSize, T_VOID **space)
{
    UINT32            excess;
    T_ULONG           realSize;
    T_ULONG           offset;
    T_TTOS_HeapBlock *theBlock;
    T_TTOS_HeapBlock *nextBlock;
    T_TTOS_HeapBlock *tmpBlock;
    T_VOID           *ptr;

    /* @REPLACE_BRACKET: space == NULL */
    if (space == NULL)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    *space = NULL;

    /* @REPLACE_BRACKET: hcb == NULL */
    if (hcb == NULL)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: (size == U(0)) || (alignedSize == U(0)) */
    if ((size == 0U) || (alignedSize == 0U))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @REPLACE_BRACKET: hcb->heap_size == U(0) */
    if (hcb->heap_size == 0U)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT: 避免某些体系结构非对齐异常 */
    /* @REPLACE_BRACKET: alignedSize < N */
    if (alignedSize < N)
    {
        alignedSize = N;
    }

    /* @KEEP_COMMENT:
     * 分配的内存块是按alignedSize大小对齐的，需要多加一个alignedSize的空间，以保障调用者能够得到alignedSize对齐的地址空间
     */
    realSize = size + alignedSize + HEAP_USED_BLOCK_OVERHEAD;
    /* @KEEP_COMMENT:
     * 检查要分配的大小是否是按alignedSize大小对齐的,没有对齐则将大小按alignedSize对齐
     */
    excess = size % alignedSize;

    /* @REPLACE_BRACKET: excess != U(0) */
    if (excess != 0U)
    {
        realSize += alignedSize - excess;
    }

    /* @REPLACE_BRACKET: realSize < size */
    if (realSize < size)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @KEEP_COMMENT:
     * 检查实际要分配的空间是否小于最小内存块，如果小于则将实际分配的空间调整为最小内存块的大小
     */
    /* @REPLACE_BRACKET: realSize < HEAP_MINIMUM_BLOCKSIZE */
    if (realSize < HEAP_MINIMUM_BLOCKSIZE)
    {
        realSize = (T_ULONG)HEAP_MINIMUM_BLOCKSIZE;
    }

    /* @KEEP_COMMENT: 从空闲链表头开始查找能够满足分配需求的空闲内存块 */
    theBlock = hcb->first;
    if (theBlock == NULL)
    {
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /* @KEEP_COMMENT: 找到链表的尾部则表明没有能够满足分配需求的内存块 */
        /* @REPLACE_BRACKET: theBlock == (T_TTOS_HeapBlock *)&hcb->final */
        if (theBlock == (T_TTOS_HeapBlock *)&hcb->final)
        {
            hcb->allocate_fails++;
            /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
            return (TTOS_UNSATISFIED);
        }

        /* @REPLACE_BRACKET: theBlock->fflag >= realSize */
        if (theBlock->fflag >= realSize)
        {
            break;
        }

        else
        {
            /* @REPLACE_BRACKET: hcb->last == theBlock */
            if (hcb->last == theBlock)
            {
                hcb->allocate_fails++;
                /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
                return (TTOS_UNSATISFIED);
            }
        }

        theBlock = theBlock->next;
    }

    if (theBlock == NULL)
    {
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:
     * 验证满足分配的内存块是否是空闲的，如果不是空闲则表明系统出现了内部空间的非法改写
     */
    /* @REPLACE_BRACKET: (theBlock->fflag & HEAP_BLOCK_USED) != UL(0) */
    if ((theBlock->fflag & HEAP_BLOCK_USED) != 0UL)
    {
        hcb->internal_errors++;
        /* @REPLACE_BRACKET: TTOS_INTERNAL_ERROR */
        return (TTOS_INTERNAL_ERROR);
    }

    /* @KEEP_COMMENT:
     * 验证满足分配的内存块是否在堆范围内,如果不在堆范围内则表明系统出现了内部空间的非法改写
     */
    /* @REPLACE_BRACKET: (theBlock > hcb->final) || (theBlock < hcb->start) */
    if ((theBlock > hcb->final) || (theBlock < hcb->start))
    {
        hcb->internal_errors++;
        /* @REPLACE_BRACKET: TTOS_INTERNAL_ERROR */
        return (TTOS_INTERNAL_ERROR);
    }

    /* @KEEP_COMMENT:
     * 找到的内存块被分配后剩余的空间还能够满足最小内存块要求，则将该内存块分为两个内存块，一个是空闲内存块，一个是已分配内存块
     */
    /* @REPLACE_BRACKET: (theBlock->fflag - realSize) >= HEAP_MINIMUM_BLOCKSIZE
     */
    if ((theBlock->fflag - realSize) >= HEAP_MINIMUM_BLOCKSIZE)
    {
        /* @KEEP_COMMENT: 得到内存块前向相邻的内存块 */
        tmpBlock = HEAP_NEXTBLOCK (theBlock);
        if (tmpBlock == NULL)
        {
            return (TTOS_INVALID_ADDRESS);
        }

        /* @KEEP_COMMENT:验证前向相邻的内存块是否是有效的 */
        /* @REPLACE_BRACKET: tmpBlock->bflag != theBlock->fflag */
        if (tmpBlock->bflag != theBlock->fflag)
        {
            hcb->internal_errors++;
            /* @REPLACE_BRACKET: TTOS_INTERNAL_ERROR */
            return (TTOS_INTERNAL_ERROR);
        }

        /* @KEEP_COMMENT: 调整前向属性，指向新的分配内存块 */
        tmpBlock->bflag = realSize | HEAP_BLOCK_USED;
        /* @KEEP_COMMENT: 调整分配后的内存块大小 */
        theBlock->fflag -= realSize;
        /* @KEEP_COMMENT:
         * 得到新的内存块，并调整期后向属性指向分配剩下的内存块，前向属性指向原有的下一个内存块
         */
        nextBlock = HEAP_NEXTBLOCK (theBlock);
        if (nextBlock == NULL)
        {
            return (TTOS_INVALID_ADDRESS);
        }
        nextBlock->bflag = theBlock->fflag;
        nextBlock->fflag = realSize | HEAP_BLOCK_USED;
        /* @KEEP_COMMENT:
         * 用户可以使用的地址为内存块前后向属性结束的地址，即空闲指针开始的地方.
         */
        ptr = (T_VOID *)&nextBlock->next;
    }

    else
    {
        /* @KEEP_COMMENT:
         * 找到的内存块不足以分裂为两个内存块时，直接将找的内存块作为整体分配 */
        nextBlock = HEAP_NEXTBLOCK (theBlock);
        if (nextBlock == NULL)
        {
            return (TTOS_INVALID_ADDRESS);
        }

        /* @REPLACE_BRACKET: nextBlock->bflag != theBlock->fflag */
        if (nextBlock->bflag != theBlock->fflag)
        {
            hcb->internal_errors++;
            /* @REPLACE_BRACKET: TTOS_INTERNAL_ERROR */
            return (TTOS_INTERNAL_ERROR);
        }

        /* @KEEP_COMMENT:
         * 将内存块的前向属性和前向相邻内存块的后向属性调整为被使用状态 */
        nextBlock->bflag |= HEAP_BLOCK_USED;
        theBlock->fflag |= HEAP_BLOCK_USED;
        /* @KEEP_COMMENT: 将内存块从空闲链表上移出 */
        theBlock->next->previous = theBlock->previous;
        theBlock->previous->next = theBlock->next;
        /* @KEEP_COMMENT: 减少了一个空闲内存块 */
        hcb->free_blocks--;
        /* @KEEP_COMMENT:
         * 用户可以使用的地址为内存块前后向属性结束的地址，即空闲指针开始的地方.
         */
        ptr = (T_VOID *)&theBlock->next;
    }

    /* @KEEP_COMMENT: 得到对齐的偏移 */
    offset = (T_ULONG)(alignedSize)
             - (((T_ULONG)ptr) & ((T_ULONG)(alignedSize)-1));
    /* @KEEP_COMMENT: 得到实际返回给调用者的空间地址 */
    ptr = (T_VOID *)(((UINT8 *)ptr) + offset);
    /* @KEEP_COMMENT:
     * 在返回空间地址的前面记录偏移值，便于释放操作定位实际的内存块位置 */
    *(((T_ULONG *)ptr) - (T_ULONG)1UL) = offset;
    /* @KEEP_COMMENT: 记录成功分配的次数 */
    hcb->allocate_goods++;
    /* @KEEP_COMMENT: 记录处于使用状态的堆空间大小 */
    hcb->in_use_size += (UINT32)realSize;
    /* @KEEP_COMMENT: 输出得到的空间地址 */
    *space = ptr;
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/*
 * @brief
 *       释放指定的内存块到指定的堆中。
 * @param      hcb: 回收被释放内存块的堆的控制结构
 * @param[in]  startingAddress: 请求释放的内存块的用户可使用空间的起始地址
 * @return
 *       TTOS_INVALID_STATE: 状态不正确
 *       TTOS_INVALID_ADDRESS: 输入参数hcb 或 startingAddress为NULL
 *       TTOS_UNSATISFIED: 不能满足请求
 *       TTOS_OK: 释放成功
 * @implements: RTE_DMEM.3
 */
T_TTOS_ReturnCode ttosFreeHeap (T_TTOS_HeapControl *hcb,
                                T_VOID             *startingAddress)
{
    T_ULONG           offset;
    T_ULONG           theSize;
    T_LONG            unoffset;
    T_TTOS_HeapBlock *theBlock;
    T_TTOS_HeapBlock *nextBlock;
    T_TTOS_HeapBlock *newNextBlock;
    T_TTOS_HeapBlock *previousBlock;
    T_TTOS_HeapBlock *temporaryBlock;

    /* @REPLACE_BRACKET: (hcb == NULL) || (startingAddress == NULL) */
    if ((hcb == NULL) || (startingAddress == NULL))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: hcb->heap_size == U(0) */
    if (hcb->heap_size == 0U)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT:
     * 分配的空间地址按页对齐的，在实际可用空间的前面会记录内存块控制结构开始地址与分配使用地址间的偏移
     */
    offset = (T_ULONG)(*(((T_ULONG *)startingAddress) - 1U)
                       + HEAP_USED_BLOCK_OVERHEAD);
    /* @KEEP_COMMENT: 得到实际要释放的内存块 */
    unoffset = (T_LONG)offset;
    unoffset = -unoffset;
    theBlock = HEAP_BLOCKAT (startingAddress, unoffset);
    if (theBlock == NULL)
    {
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 验证要释放内存块是否在堆范围内 */
    /* @REPLACE_BRACKET: (theBlock > hcb->final) || (theBlock < hcb->start) */
    if ((theBlock > hcb->final) || (theBlock < hcb->start))
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @KEEP_COMMENT: 要释放的内存块应该是处于使用状态  */
    /* @REPLACE_BRACKET: (theBlock->fflag &  HEAP_BLOCK_USED ) == UL(0) */
    if ((theBlock->fflag & HEAP_BLOCK_USED) == 0U)
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* 要释放内存块的实际大小 */
    theSize = theBlock->fflag & (~HEAP_BLOCK_USED);
    /* @KEEP_COMMENT: 定位释放内存块的前向相邻块 */
    nextBlock = HEAP_BLOCKAT (theBlock, theSize);
    if (nextBlock == NULL)
    {
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 检查相邻块是否在堆范围内 */
    /* @REPLACE_BRACKET: (nextBlock > hcb->final) || (nextBlock < hcb->start) */
    if ((nextBlock > hcb->final) || (nextBlock < hcb->start))
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @KEEP_COMMENT: 检查前后向属性是否一致 */
    /* @REPLACE_BRACKET: theBlock->fflag != nextBlock->bflag */
    if (theBlock->fflag != nextBlock->bflag)
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @KEEP_COMMENT: 如果后向相邻块是空闲的则需要合并后向的内存块 */
    /* @REPLACE_BRACKET: (theBlock->bflag & HEAP_BLOCK_USED ) == UL(0) */
    if ((theBlock->bflag & HEAP_BLOCK_USED) == 0UL)
    {
        /* @KEEP_COMMENT: 计算后向相邻块的位置*/
        offset        = (T_ULONG)(theBlock->bflag & (~HEAP_BLOCK_USED));
        unoffset      = (T_LONG)offset;
        unoffset      = -unoffset;
        previousBlock = HEAP_BLOCKAT (theBlock, unoffset);

        /* @KEEP_COMMENT: 检查后向相邻块是否在堆空间范围内 */
        /* @REPLACE_BRACKET: (previousBlock > hcb->final) || (previousBlock <
         * hcb->start) */
        if ((previousBlock > hcb->final) || (previousBlock < hcb->start))
        {
            hcb->free_fails++;
            /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
            return (TTOS_UNSATISFIED);
        }

        /* @KEEP_COMMENT: 如果前向相邻块也是空闲则需要同时合并  */
        /* @REPLACE_BRACKET: TTOS_OK */
        if ((nextBlock->fflag & HEAP_BLOCK_USED) == 0UL)
        {
            previousBlock->fflag += nextBlock->fflag + theSize;
            temporaryBlock        = HEAP_NEXTBLOCK (previousBlock);
            temporaryBlock->bflag = previousBlock->fflag;
            /* @KEEP_COMMENT: 把前向相邻块从空闲队列中移出，因为该块已经被合并了
             */
            nextBlock->next->previous = nextBlock->previous;
            nextBlock->previous->next = nextBlock->next;
            /* @KEEP_COMMENT: 减少了一个空闲内存块 */
            hcb->free_blocks--;
        }

        else
        {
            /* @KEEP_COMMENT:
             * 只有后向相邻块是空闲的，则只需要调整后向相邻块的前向属性和前向相邻块的后向属性
             */
            nextBlock->bflag     = previousBlock->fflag + theSize;
            previousBlock->fflag = nextBlock->bflag;
        }
    }

    else
    {
        /* 后向相邻块不是空闲的 */
        /* @KEEP_COMMENT: 如果前向相邻块是空闲则需要合并 */
        /* @REPLACE_BRACKET: (nextBlock->fflag & HEAP_BLOCK_USED ) == UL(0) */
        if ((nextBlock->fflag & HEAP_BLOCK_USED) == 0UL)
        {
            theBlock->fflag           = theSize + nextBlock->fflag;
            newNextBlock              = HEAP_NEXTBLOCK (theBlock);
            newNextBlock->bflag       = theBlock->fflag;
            theBlock->next            = nextBlock->next;
            theBlock->previous        = nextBlock->previous;
            nextBlock->previous->next = theBlock;
            nextBlock->next->previous = theBlock;
        }

        else
        {
            /* 前后向相邻块都不空闲 */
            /* @KEEP_COMMENT: 调整属性标志 */
            theBlock->fflag  = theSize;
            nextBlock->bflag = theBlock->fflag;
            /* @KEEP_COMMENT: 释放的内存块放到空闲队列的首部 */
            theBlock->previous       = (T_TTOS_HeapBlock *)(&hcb->start);
            theBlock->next           = hcb->first;
            hcb->first               = theBlock;
            theBlock->next->previous = theBlock;
            /* @KEEP_COMMENT: 增加了一个空闲内存块 */
            hcb->free_blocks++;
        }
    }

    /* @KEEP_COMMENT: 记录成功释放的次数 */
    hcb->free_goods++;
    /* @KEEP_COMMENT: 更新处于使用状态的堆空间大小 */
    hcb->in_use_size -= (UINT32)theSize;

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

T_TTOS_ReturnCode ttosGetBlockSizeHeap (T_TTOS_HeapControl *hcb,
                                        T_VOID *startingAddress, size_t *size)
{
    T_ULONG           offset;
    T_ULONG           theSize;
    T_LONG            unoffset;
    T_TTOS_HeapBlock *theBlock;
    T_TTOS_HeapBlock *nextBlock;
    T_TTOS_HeapBlock *newNextBlock;
    T_TTOS_HeapBlock *previousBlock;
    T_TTOS_HeapBlock *temporaryBlock;

    /* @REPLACE_BRACKET: (hcb == NULL) || (startingAddress == NULL) */
    if ((hcb == NULL) || (startingAddress == NULL))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: hcb->heap_size == U(0) */
    if (hcb->heap_size == 0U)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT:
     * 分配的空间地址按页对齐的，在实际可用空间的前面会记录内存块控制结构开始地址与分配使用地址间的偏移
     */
    offset = (T_ULONG)(*(((T_ULONG *)startingAddress) - 1U)
                       + HEAP_USED_BLOCK_OVERHEAD);
    /* @KEEP_COMMENT: 得到实际要释放的内存块 */
    unoffset = (T_LONG)offset;
    unoffset = -unoffset;
    theBlock = HEAP_BLOCKAT (startingAddress, unoffset);
    if (theBlock == NULL)
    {
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 验证要释放内存块是否在堆范围内 */
    /* @REPLACE_BRACKET: (theBlock > hcb->final) || (theBlock < hcb->start) */
    if ((theBlock > hcb->final) || (theBlock < hcb->start))
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @KEEP_COMMENT: 要释放的内存块应该是处于使用状态  */
    /* @REPLACE_BRACKET: (theBlock->fflag &  HEAP_BLOCK_USED ) == UL(0) */
    if ((theBlock->fflag & HEAP_BLOCK_USED) == 0U)
    {
        hcb->free_fails++;
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* 要释放内存块的实际大小 */
    theSize = theBlock->fflag & (~HEAP_BLOCK_USED);

    *size = theSize;

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
