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

#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum object_type
{
    TRACE_OBJ_KTHREAD = 1,
    TRACE_OBJ_THREAD,
    TRACE_OBJ_PROCESS,
    TRACE_OBJ_IRQ,
    TRACE_OBJ_STRING,
};

enum compete_type
{
    COMPETE_T_INT = 1,
    COMPETE_T_SPINLOCK,
};

#if defined(CONFIG_ARCH_ARM)
#define INT_COMPETE_TICK 120 // 24M / 5us
#define SPINLOCK_COMPETE_TICK 120
#elif defined(CONFIG_X86_64)
#define INT_COMPETE_TICK 15000      // 3G / 5us
#define SPINLOCK_COMPETE_TICK 15000 // 3G / 5us
#endif

struct trace_slot;

struct trace_ops
{
    void (*init)(struct trace_slot *slot);
    void (*irq)(uint32_t virq, uint64_t stimestamp, uint32_t duration);
    void (*log)(uint32_t level, char *tag, char *msg, uint32_t len);
    void (*mark)(const void *name_point, uint32_t begin);
    void (*object)(uint32_t type, void *data);
    void (*syscall)(uint32_t id, uint32_t is_entry, void *args);
    void (*switch_task)(void *from, void *to);
    void (*waking_task)(void *task);
    void (*task_bindcpu)(void *task);
    void (*signal_generate)(int32_t code, uint32_t pgid, uint32_t pid, int32_t sig, int32_t result);
    void (*signal_deliver)(int32_t code, int32_t sig, int32_t sa_flags);
    void (*malloc)(uint32_t alloc_size, uint32_t req_size, uint32_t flag, void *call_site,
                   void *ptr);
    void (*free)(void *call_site, void *ptr);
    void (*exec)(char *filename);
    void (*fork)(void *child_task);
    void (*exit)(void);
    void (*compete)(void *call_site, uint32_t type, uint32_t duration);
    void (*config)(struct trace_slot *slot);
};

struct trace_slot
{
    bool used;
    uint32_t ops_bit;
    struct trace_ops *ops;
    struct trace_ops *back_ops;
    void *seq_data;
    void *priv;
};

#if !defined(CONFIG_TRACE) &&                                                                      \
    (defined(CONFIG_TRACE_MARK) || defined(CONFIG_TRACE_IRQ) || defined(CONFIG_TRACE_SYSCALL) ||   \
     defined(CONFIG_TRACE_LOG) || defined(CONFIG_TRACE_SIGNAL) || defined(CONFIG_TRACE_MEMORY) ||  \
     defined(CONFIG_TRACE_COMPETE))
#error                                                                                             \
    "Trace modules are enabled but CONFIG_TRACE is not defined. Please enable CONFIG_TRACE first."
#endif

/* 条件跟踪宏 */
#if defined(CONFIG_TRACE)
#define trace_object(type, data) _trace_object(type, data)
#define trace_task_bindcpu(task) _trace_task_bindcpu(task)
#define trace_switch_task(old_task, new_task) _trace_switch_task(old_task, new_task)
#define trace_waking_task(task) _trace_waking_task(task)
#define trace_exec(filename) _trace_exec(filename)
#define trace_fork(child_task) _trace_fork(child_task)
#define trace_exit() _trace_exit()
#else
#define trace_object(type, data)
#define trace_task_bindcpu(task)
#define trace_switch_task(old_task, new_task)
#define trace_waking_task(task)
#define trace_exec(filename)
#define trace_fork(child_task)
#define trace_exit()
#endif

#ifdef CONFIG_TRACE_MARK
#define trace_mark_begin(point) _trace_mark_begin(point)
#define trace_mark_end(point) _trace_mark_end(point)
#else
#define trace_mark_begin(point)
#define trace_mark_end(point)
#endif

#ifdef CONFIG_TRACE_IRQ
#define trace_irq(virq, stimestamp, duration) _trace_irq(virq, stimestamp, duration)
#else
#define trace_irq(virq, stimestamp, duration)
#endif

#ifdef CONFIG_TRACE_SYSCALL
#define trace_syscall(id, is_entry, data) _trace_syscall(id, is_entry, data)
#define trace_syscall_args_init(buf, arg0, arg1, arg2, arg3, arg4, arg5)                           \
    {                                                                                              \
        buf[0] = arg0;                                                                             \
        buf[1] = arg1;                                                                             \
        buf[2] = arg2;                                                                             \
        buf[3] = arg3;                                                                             \
        buf[4] = arg4;                                                                             \
        buf[5] = arg5;                                                                             \
    }
#else
#define trace_syscall(id, is_entry, data)
#define trace_syscall_args_init(buf, arg0, arg1, arg2, arg3, arg4, arg5)
#endif

#ifdef CONFIG_TRACE_LOG
#define trace_log(level, tag, msg, len) _trace_log(level, tag, msg, len)
#else
#define trace_log(level, tag, msg, len)
#endif

#ifdef CONFIG_TRACE_SIGNAL
#define trace_signal_generate(code, pgid, pid, sig, result)                                        \
    _trace_signal_generate(code, pgid, pid, sig, result)
#define trace_signal_deliver(code, sig, sa_flags) _trace_signal_deliver(code, sig, sa_flags)
#else
#define trace_signal_generate(code, pgid, pid, sig, result)
#define trace_signal_deliver(code, sig, sa_flags)
#endif

#ifdef CONFIG_TRACE_MEMORY
#define trace_malloc(alloc_size, req_size, flag, call_site, ptr)                                   \
    _trace_malloc(alloc_size, req_size, flag, call_site, ptr)
#define trace_free(call_site, ptr) _trace_free(call_site, ptr)
#else
#define trace_malloc(alloc_size, req_size, flag, call_site, ptr)
#define trace_free(call_site, ptr)
#endif

#ifdef CONFIG_TRACE_COMPETE
#define trace_compete(call_site, type, duration) _trace_compete(call_site, type, duration)
#else
#define trace_compete(call_site, type, duration)
#endif

int trace_register(const char *name, struct trace_ops *ops);
void _trace_object(uint32_t type, void *data);
void _trace_task_bindcpu(void *task);
void _trace_switch_task(void *old_task, void *new_task);
void _trace_waking_task(void *task);
void _trace_mark_begin(const void *point);
void _trace_mark_end(const void *point);
void _trace_irq(uint32_t virq, uint64_t stimestamp, uint32_t duration);
void _trace_syscall(uint32_t id, uint32_t is_entry, void *data);
void _trace_log(uint32_t level, char *tag, char *msg, uint32_t len);
void _trace_signal_generate(int32_t code, uint32_t pgid, uint32_t pid, int32_t sig, int32_t result);
void _trace_signal_deliver(int32_t code, int32_t sig, int32_t sa_flags);
void _trace_malloc(uint32_t alloc_size, uint32_t req_size, uint32_t flag, void *call_site,
                   void *ptr);
void _trace_free(void *call_site, void *ptr);
void _trace_exec(char *filename);
void _trace_fork(void *child_task);
void _trace_exit(void);
void _trace_compete(void *call_site, uint32_t type, uint32_t duration);
#endif /* __TRACE_H__ */