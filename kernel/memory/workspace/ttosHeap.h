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
 * 变更历史
 * 2012-10-12  周英豪
 * ttosAllocateHeap的注释中返回值TTOS_INVALID_SIZE增加对计算实际分配空间大小超出0xFFFFFFFF的情况。
 * 2012-04-21  叶俊   增加mslHeapInit返回TTOS_INVALID_ADDRESS的描述。(BUG6195)
 * 2010-08-30  杨曦   创建该文件.
 */

/*
 * @file  ttosHeap.h
 * @brief
 *       功能:
 *       <li>定义堆相关的数据与结构，以及堆相关操作.</li>
 */

#ifndef TTOSHEAP_H
#define TTOSHEAP_H

/* 头文件 */
#include <ttos.h>
#include <ttosTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* 类型定义 */

/* 定义堆块结构 */
typedef struct T_TTOS_HeapBlock_Struct T_TTOS_HeapBlock;
struct T_TTOS_HeapBlock_Struct {
    T_ULONG           bflag;    /* 后向内存块的大小和状态 */
    T_ULONG           fflag;    /* 前向内存块的大小和状态 */
    T_TTOS_HeapBlock *next;     /* 指向后续空闲内存块 */
    T_TTOS_HeapBlock *previous; /* 指向前向空闲内存块 */
};

/* 定义堆管理结构 */
typedef struct {
    T_TTOS_HeapBlock *start; /* 指向堆的头部内存块 */
    T_TTOS_HeapBlock *final; /* 指向堆的尾部内存块 */

    T_TTOS_HeapBlock *first;       /* 指向堆的第一个空闲内存块 */
    T_TTOS_HeapBlock *null;        /* 始终指向空地址 */
    T_TTOS_HeapBlock *last;        /* 指向堆的最后一个空闲内存块 */
    UINT32            heap_size;   /* 堆空间的大小 */
    UINT32            in_use_size; /* 正在使用的堆空间大小 */
    UINT32            free_blocks; /* 处于空闲的内存块数 */
    UINT32            allocate_goods;  /* 累计成功分配次数 */
    UINT32            free_goods;      /* 累计成功释放次数 */
    UINT32            allocate_fails;  /* 累计分配失败的次数 */
    UINT32            free_fails;      /* 累计释放失败的次数 */
    UINT32            internal_errors; /* 累计内部错误数 */
} T_TTOS_HeapControl;

/* 接口声明 */

T_TTOS_ReturnCode ttosInitHeap (T_TTOS_HeapControl *hcb, void *startingAddress,
                                UINT32 heapSize);
T_TTOS_ReturnCode ttosAllocateHeap (T_TTOS_HeapControl *hcb, UINT32 size,
                                    UINT32 alignedSize, void **space);
T_TTOS_ReturnCode ttosFreeHeap (T_TTOS_HeapControl *hcb, void *startingAddress);
T_TTOS_ReturnCode ttosGetBlockSizeHeap (T_TTOS_HeapControl *hcb,
                                        T_VOID *startingAddress, size_t *size);

#ifdef __cplusplus
}
#endif

#endif
