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

#ifndef _FUTEX_H__
#define _FUTEX_H__

#include <list.h>
#include <rt_mutex.h>
#include <stdint.h>

typedef struct T_TTOS_ProcessControlBlock *pcb_t;
struct futex_pi_state;

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_WAIT_REQUEUE_PI 11
#define FUTEX_CMP_REQUEUE_PI 12
#define FUTEX_LOCK_PI2 13

#define FUTEX_PRIVATE_FLAG 128

#define FUTEX_CLOCK_REALTIME 256

#define FUTEX_WAITERS 0x80000000
#define FUTEX_OWNER_DIED 0x40000000
#define FUTEX_TID_MASK 0x3fffffff

#define FLAGS_SIZE_8 0x0000
#define FLAGS_SIZE_16 0x0001
#define FLAGS_SIZE_32 0x0002
#define FLAGS_SIZE_64 0x0003

#define FLAGS_SIZE_MASK 0x0003

#define FLAGS_SHARED 0x0010

#define FLAGS_CLOCKRT 0x0020
#define FLAGS_HAS_TIMEOUT 0x0040
#define FLAGS_NUMA 0x0080
#define FLAGS_STRICT 0x0100

struct robust_list
{
    struct robust_list *next;
};

struct robust_list_head
{
    struct robust_list list;
    long futex_offset;
    struct robust_list *list_op_pending;
};

struct futex_key
{
    uint64_t pid;
    uint64_t uaddr;
};

struct futex_q
{
    struct list_head list;
    pcb_t pcb;
    struct futex_key key;
    uint32_t bitset;
    struct futex_pi_state *pi_state;
    unsigned int flags;
};

struct futex_pi_state
{
    struct list_head pi_list;
    struct futex_key key;
    struct rt_mutex rt_mutex;
    struct rt_mutex *pi;
    TASK_ID owner;
    int owner_tid;
    int waiters;
    int owner_died;
    int refcount;
};

/*
 * bitset with all bits set for the FUTEX_xxx_BITSET OPs to request a
 * match of any bit.
 */
#define FUTEX_BITSET_MATCH_ANY 0xffffffff

#endif /* _FUTEX_H__ */
