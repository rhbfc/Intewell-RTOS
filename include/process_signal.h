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

#ifndef _PROCESS_SIGNAL_H
#define _PROCESS_SIGNAL_H

/************************头文件********************************/
#include <list.h>
#include <signal.h>
#include <spinlock.h>
#include <sys/types.h>
#include <system/types.h>
#include <time/ktime.h>
#include <bitsperlong.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/

/* 信号投递目标 */
#define TO_THREAD  1
#define TO_PROCESS 2
#define TO_PGROUP  3

/* 信号编号上限 */
#define _NSIG      64
#define SIGNAL_MAX 64

#ifndef SIGRTMIN
#define SIGRTMIN 32
#endif

#ifndef SIGRTMAX
#define SIGRTMAX (_NSIG - 1)
#endif

/* siginfo si_code */
#define SI_CODE_POSIX_TIMER 0x100
#define SI_ASYNCNL  (-60)
#define SI_TKILL    (-6)
#define SI_SIGIO    (-5)
#define SI_ASYNCIO  (-4)
#define SI_MESGQ    (-3)
#define SI_TIMER    (-2)
#define SI_QUEUE    (-1)
#define SI_USER     0
#define SI_KERNEL   128

/* sigprocmask how */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIG_HOLD ((void (*)(int))2)

/* sigaltstack ss_flags */
#define SS_ONSTACK   1
#define SS_DISABLE   2
#define SS_AUTODISARM (1U << 31) /* disable sas during sighandling */
#define SS_FLAG_BITS  SS_AUTODISARM

/* SA flags */
#ifndef SA_NODEFER
#define SA_NODEFER 0x40000000
#endif
#define SA_ONESHOT SA_RESETHAND

/* sigev_notify */
#define SIGEV_SIGNAL    0
#define SIGEV_NONE      1
#define SIGEV_THREAD    2
#define SIGEV_THREAD_ID 4

/* 上下文类型 */
#define CONTEXT_NONE       0
#define SYSCALL_CONTEXT    1
#define IRQ_CONTEXT        2
#define EXCEPTION_CONTEXT  3

/* 扩展 errno */
#define ENOIOCTLCMD 515 /* No ioctl command */
#define EPROBE_DEFER 517 /* Driver requests probe retry */
#define EOPENSTALE  518 /* open found a stale dentry */
#define ENOPARAM    519 /* Parameter not supported */

/* SIGFPE si_code */
#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8

/* SIGILL si_code */
#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8

/* SIGSEGV si_code */
#define SEGV_MAPERR  1
#define SEGV_ACCERR  2
#define SEGV_BNDERR  3
#define SEGV_PKUERR  4
#define SEGV_MTEAERR 8
#define SEGV_MTESERR 9

/* SIGBUS si_code */
#define BUS_ADRALN    1
#define BUS_ADRERR    2
#define BUS_OBJERR    3
#define BUS_MCEERR_AR 4
#define BUS_MCEERR_AO 5

/* SIGCHLD si_code */
#define CLD_EXITED    1
#define CLD_KILLED    2
#define CLD_DUMPED    3
#define CLD_TRAPPED   4
#define CLD_STOPPED   5
#define CLD_CONTINUED 6

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))
#endif

#define __user

/************************类型定义******************************/
typedef struct T_TTOS_ProcessControlBlock *pcb_t;
typedef struct T_TTOS_TaskControlBlock_Struct T_TTOS_TaskControlBlock;

struct tty_struct;

typedef union sigval {
    int   sival_int; /* Integer signal value */
    void *sival_ptr; /* Pointer signal value */
} sigval_t;

typedef struct siginfo
{
#ifdef __SI_SWAP_ERRNO_CODE
    int si_signo, si_code, si_errno;
#else
    int si_signo, si_errno, si_code;
#endif
    union {
        char __pad[128 - 2 * sizeof(int) - sizeof(long)];
        struct
        {
            union {
                struct
                {
                    pid_t si_pid;
                    uid_t si_uid;
                } __piduid;
                struct
                {
                    int si_timerid;
                    int si_overrun;
                } __timer;
            } __first;
            union {
                union sigval si_value;
                struct
                {
                    int si_status;
                    clock_t si_utime, si_stime;
                } __sigchld;
            } __second;
        } __si_common;
        struct
        {
            void *si_addr;
            short si_addr_lsb;
            union {
                struct
                {
                    void *si_lower;
                    void *si_upper;
                } __addr_bnd;
                unsigned si_pkey;
            } __first;
        } __sigfault;
        struct
        {
            long si_band;
            int  si_fd;
        } __sigpoll;
        struct
        {
            void        *si_call_addr;
            int          si_syscall;
            unsigned int si_arch;
        } __sigsys;
    } __si_fields;
} ksiginfo_t;

typedef struct siginfo siginfo_t;

/* siginfo 字段访问宏 */
#define si_pid      __si_fields.__si_common.__first.__piduid.si_pid
#define si_uid      __si_fields.__si_common.__first.__piduid.si_uid
#define si_status   __si_fields.__si_common.__second.__sigchld.si_status
#define si_utime    __si_fields.__si_common.__second.__sigchld.si_utime
#define si_stime    __si_fields.__si_common.__second.__sigchld.si_stime
#define si_value    __si_fields.__si_common.__second.si_value
#define si_addr     __si_fields.__sigfault.si_addr
#define si_addr_lsb __si_fields.__sigfault.si_addr_lsb
#define si_lower    __si_fields.__sigfault.__first.__addr_bnd.si_lower
#define si_upper    __si_fields.__sigfault.__first.__addr_bnd.si_upper
#define si_pkey     __si_fields.__sigfault.__first.si_pkey
#define si_band     __si_fields.__sigpoll.si_band
#define si_fd       __si_fields.__sigpoll.si_fd
#define si_timerid  __si_fields.__si_common.__first.__timer.si_timerid
#define si_overrun  __si_fields.__si_common.__first.__timer.si_overrun
#define si_ptr      si_value.sival_ptr
#define si_int      si_value.sival_int
#define si_call_addr __si_fields.__sigsys.si_call_addr
#define si_syscall  __si_fields.__sigsys.si_syscall
#define si_arch     __si_fields.__sigsys.si_arch

struct k_sigaction
{
    union {
        /* 当sa_flags中没有设置SA_SIGINFO标志时，使用该handler */
        void (*_sa_handler)(int);
        /* 当sa_flags中设置了SA_SIGINFO标志时，使用该handler */
        void (*_sa_sigaction)(int, siginfo_t *, void *);
    } __sa_handler;

    unsigned long sa_flags;
    void (*sa_restorer)(void);

    /* 在信号处理函数执行期间要阻塞的信号集 */
    unsigned mask[2];
};

struct ksignal
{
    struct k_sigaction ka;
    siginfo_t info;
    int sig;
};

struct ksigevent
{
    union sigval sigev_value;
    int sigev_signo;
    int sigev_notify;
    int sigev_tid;
};

typedef void (*sighandler_t)(int);

/************************信号集合宏*****************************/
/*
 * sigmask() 用于生成“当前机器字宽”下的基础位掩码。
 * signal_long_mask() 强调返回值要落在单个 long 对应的信号位图布局里：
 *   - 当实时信号范围已经超过一个 long 时，用 64-bit 常量避免中间计算被截断
 *   - 否则直接复用 sigmask() 即可
 */
#define sigmask(sig) (1UL << ((sig)-1))

#if SIGRTMIN > BITS_PER_LONG
#define signal_long_mask(sig) (1ULL << ((sig)-1))
#else
#define signal_long_mask(sig) sigmask(sig)
#endif

#define PROCESS_SIG_JOBCTL_SET \
    (signal_long_mask(SIGCONT) | signal_long_mask(SIGSTOP) | signal_long_mask(SIGTSTP) | \
     signal_long_mask(SIGTTIN) | signal_long_mask(SIGTTOU))

#define PROCESS_SIG_STOP_SET (\
	signal_long_mask(SIGSTOP)   |  signal_long_mask(SIGTSTP)   | \
	signal_long_mask(SIGTTIN)   |  signal_long_mask(SIGTTOU)   )

#define PROCESS_SIG_IGNORE_SET (\
        signal_long_mask(SIGCONT)   |  signal_long_mask(SIGCHLD)   | \
	signal_long_mask(SIGWINCH)  |  signal_long_mask(SIGURG)    )

/* k_sigset_t 布局宏 */
#define K_SIGSET_NLONG (SIGNAL_MAX / BITS_PER_LONG)
#define SIGSET_INDEX(sig) ((sig) / BITS_PER_LONG) /* sig 所在的 long 下标 */
#define SIGSET_BIT(sig)   ((sig) % BITS_PER_LONG) /* sig 在该 long 中的位偏移 */

typedef struct
{
    unsigned long sig[K_SIGSET_NLONG];
} k_sigset_t;

#define SIG_ACT_DFL ((sighandler_t)0)
#define SIG_ACT_IGN ((sighandler_t)1)

/************************结构体定义****************************/

typedef struct ksiginfo_entry
{
    struct list_head node;
    ksiginfo_t ksiginfo;
    void *priv;
} ksiginfo_entry_t;

struct k_sigpending_queue
{
    struct list_head siginfo_list;
    k_sigset_t sigset_pending;
};

struct ktimer;
struct rb_root;

struct shared_signal
{
    ttos_spinlock_t siglock;
    bool need_restore_sigset_mask;

    /* itimer 相关 */
    struct ktimer *itimer;   /* 整个进程的 ITIMER_REAL 定时器 */
    ktime_t itimer_interval; /* itimer 的间隔时间 */

    /* posix timer 相关 */
    atomic_t        next_posix_timer_id; /* 进程的 posix-timer id */
    struct rb_root *posix_timer_root;    /* 进程的 posix-timer 树 */
    unsigned int    assign_timer_id:1;   /* posix-timers 的 id 是否使用户传入的 id */

    struct k_sigpending_queue sig_queue;

    void *ctty; /* 控制终端 */

    k_sigset_t sigchld_nostop_set; /* 子进程停止或继续时不生成 SIGCHLD */
    k_sigset_t sigchld_nowait_set; /* 子进程死亡时不产生僵尸进程 */

    pid_t tty_old_pgrp;       /* 旧的前台进程组 */
    struct tty_struct *tty;   /* 控制终端 */
};

struct shared_sighand
{
    ttos_spinlock_t siglock;
    struct k_sigaction sig_action[_NSIG];
};

struct signal_wqueue
{
    unsigned int flag;
    struct list_head waiting_list;
    ttos_spinlock_t spinlock;
};
typedef struct signal_wqueue signal_wqueue_t;

struct process_notify
{
    void (*notify)(signal_wqueue_t *signalfd_queue, int signo);
    signal_wqueue_t *signalfd_queue;
    struct list_head list_node;
};

#ifndef __ARCH_SIGEV_PREAMBLE_SIZE
#define __ARCH_SIGEV_PREAMBLE_SIZE (sizeof(int) * 2 + sizeof(sigval_t))
#endif
#define SIGEV_MAX_SIZE 64
#define SIGEV_PAD_SIZE ((SIGEV_MAX_SIZE - __ARCH_SIGEV_PREAMBLE_SIZE) / sizeof(int))

typedef struct sigevent {
    sigval_t sigev_value;
    int sigev_signo;
    int sigev_notify;
    union {
        int _pad[SIGEV_PAD_SIZE];
        int _tid;
        struct {
            void (*_function)(sigval_t);
            void *_attribute; /* really pthread_attr_t */
        } _sigev_thread;
    } _sigev_un;
} sigevent_t;

/************************接口声明******************************/
/*
 * 本头文件提供 signal 子系统对外暴露的正式接口：
 *   - syscall / arch 入口需要的能力
 *   - 通用信号类型、位图工具与基础状态接口
 * 子系统内部 helper 请放到 signal_internal.h。
 */

/* Process signal operations */
int fork_signal(unsigned long clone_flags, pcb_t child);
int signal_reset(pcb_t pcb);

/* Signal sending and delivery */
int kernel_signal_send(pid_t pid, int to, long signo, long code, ksiginfo_t *kinfo);
void process_signal_kill_to(pcb_t pcb, long sig);
int kernel_signal_action(int signo, const struct k_sigaction *act, struct k_sigaction *oact);
void signal_delivered(struct ksignal *ksig, int stepping);

/* Signal queue operations */
void pcb_signal_pending_set(pcb_t pcb);
void pcb_signal_pending_clear(pcb_t pcb);
bool pcb_signal_pending(pcb_t pcb);

/* Alt stack operations */
unsigned long signal_sp_get(unsigned long sp, struct ksignal *ksignal);
void altstack_save_to_user(stack_t __user *uss, unsigned long sp);
int altstack_restore_from_user(const stack_t *uss, unsigned long sp);
int kernel_sigaltstack(const stack_t *ss, stack_t *oss, unsigned long sp, size_t min_ss_size);

/* Sigmask operations */
void signal_mask_apply_current(k_sigset_t *newset);
k_sigset_t *signal_mask_save_target(void);
void signal_mask_restore_saved(void);
int kernel_sigprocmask(int how, k_sigset_t *newset, k_sigset_t *oldset);

/* Signal set operations */
void signal_set_or(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1);
void signal_set_not(k_sigset_t *dset, const k_sigset_t *set);
bool signal_set_is_empty(k_sigset_t *set);
void signal_set_and(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1);
bool signal_set_contains(const k_sigset_t *set, int sig_num);
bool signal_set_equal(const k_sigset_t *set1, const k_sigset_t *set2);
void signal_set_add(k_sigset_t *set, int sig_num);
void signal_set_del(k_sigset_t *set, int sig_num);
void signal_set_clear(k_sigset_t *set);
void signal_set_del_immutable(k_sigset_t *set);
void signal_set_and_not(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1);
k_sigset_t signal_set_from_long_mask(unsigned long mask);

/* Architecture-specific signal handling */
int rt_sigreturn(struct arch_context *regs);
int arch_do_signal(struct arch_context *regs);
bool in_syscall(struct arch_context *context);
void do_syscall_restart_check(struct arch_context *context, struct ksignal *ksignal);
void clear_syscall_context(struct arch_context *context);
void kernel_signal_suspend(const k_sigset_t *set);
bool signal_fetch_deliverable(struct ksignal *ksig);

/* Signal info operations */
int kernel_sigtimedwait(const k_sigset_t *sigset, siginfo_t *info,
                        const struct timespec64 *timeout, size_t sigsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _PROCESS_SIGNAL_H */
