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

#include <errno.h>
#include <signal.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <tglock.h>
#include <uaccess.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

/* ==================== sigaction ==================== */
sighandler_t sigaction_handler_get(const struct k_sigaction *action)
{
    return action->__sa_handler._sa_handler;
}

static bool sigaction_handler_ignored(pcb_t pcb, int signo)
{
    struct k_sigaction *sigaction_ptr = sigaction_get_ptr(pcb, signo);
    sighandler_t handler = sigaction_handler_get(sigaction_ptr);

    k_sigset_t ign_set = signal_set_from_long_mask(PROCESS_SIG_IGNORE_SET);

    return handler == SIG_IGN || (handler == SIG_DFL && signal_set_contains(&ign_set, signo));
}

int kernel_signal_action(int signo, const struct k_sigaction *restrict act,
                          struct k_sigaction *restrict oact)
{
    irq_flags_t irq_flag = 0;
    bool new_handler_ignored = false;
    k_sigset_t remove_sigset = {0};
    pcb_t pcb = ttosProcessSelf();
    struct shared_signal *signal = signal_shared_get(pcb);
    struct k_sigaction *sigaction_ptr = NULL;

    if (!signal_num_valid(signo) || (act && sig_immutable(signo)))
    {
        return -EINVAL;
    }

    sighand_lock(pcb, &irq_flag);

    sigaction_ptr = sigaction_get_ptr(pcb, signo);

    if (oact)
    {
        *oact = *sigaction_ptr;
    }

    if (act)
    {
        signal_set_del_immutable((k_sigset_t *)&act->mask);

        *sigaction_ptr = *act;

        if (signo == SIGCHLD)
        {
            if (act->sa_flags & SA_NOCLDSTOP)
            {
                signal_set_add(&signal->sigchld_nostop_set, SIGCHLD);
            }
            else
            {
                signal_set_del(&signal->sigchld_nostop_set, SIGCHLD);
            }
        }

        new_handler_ignored = sigaction_handler_ignored(pcb, signo);

        sighand_unlock(pcb, &irq_flag);

        /*
         * 按 POSIX 语义，某个信号一旦被改成 SIG_IGN，当前已经 pending 的同号信号
         * 也应立即丢弃，而不是等它以后出队时再决定。
         */
        if (new_handler_ignored)
        {
            signal_set_add(&remove_sigset, signo);

            sigqueue_remove_shared(pcb, &remove_sigset);

            sigqueue_remove_private(pcb, &remove_sigset);
        }
    }
    else
    {
        sighand_unlock(pcb, &irq_flag);
    }

    return 0;
}

/* ==================== 备用信号栈 (sigaltstack) ==================== */

/**
 * signal_sp_get - 计算信号处理函数的栈指针
 * @sp:      当前用户栈指针
 * @ksignal: 待投递的信号（含 sa_flags）
 *
 * 若信号的 sa_flags 设置了 SA_ONSTACK，且当前不在备用栈上，
 * 则返回备用栈的起始地址（栈向下生长时为 altstack_base + altstack_size）；
 * 否则直接返回原始栈指针 sp。
 */
unsigned long signal_sp_get(unsigned long sp, struct ksignal *ksignal)
{
    pcb_t pcb = ttosProcessSelf();

    /*
     * SA_ONSTACK 要求 handler 在备用信号栈上运行；如果当前 sp 还不在
     * 备用栈范围内，就切换到备用栈顶。该架构栈向下生长，因此入口位置
     * 取 altstack_base + altstack_size。
     */
    if (unlikely((ksignal->ka.sa_flags & SA_ONSTACK)) && !altstack_flags(sp))
    {
        return pcb->altstack_base + pcb->altstack_size;
    }

    return sp;
}

/**
 * altstack_sp_in_range - 判断 sp 是否位于备用信号栈地址范围内（不检查 SS_AUTODISARM）
 * @sp: 要检测的栈指针
 *
 * 返回非零表示 sp 在备用栈内。
 * 注意：栈向下生长时，有效范围为 (altstack_base, altstack_base + altstack_size]。
 */
int altstack_sp_in_range(unsigned long sp)
{
    pcb_t pcb = ttosProcessSelf();
#ifdef CONFIG_STACK_GROWSUP
    return sp >= pcb->altstack_base && sp - pcb->altstack_base < pcb->altstack_size;
#else
    return sp > pcb->altstack_base && sp - pcb->altstack_base <= pcb->altstack_size;
#endif
}

/**
 * altstack_on_stack - 判断当前是否运行在备用信号栈上
 * @sp: 要检测的栈指针
 *
 * 若备用栈设置了 SS_AUTODISARM，则固定返回 0：
 * SS_AUTODISARM 栈在信号进入时被内核自动解除，因此在处理函数运行期间
 * 备用栈已不再有效；如果用户状态损坏导致栈指针恰好指向备用栈末端附近，
 * 该逻辑可使信号得以继续被处理，提高可靠性。
 */
int altstack_on_stack(unsigned long sp)
{
    pcb_t pcb = ttosProcessSelf();

    if (pcb->altstack_flags & SS_AUTODISARM)
        return 0;

    return altstack_sp_in_range(sp);
}

/**
 * altstack_flags - 查询备用栈状态标志
 * @sp: 当前栈指针
 *
 * 返回值：
 *   SS_DISABLE  — 备用栈未设置（altstack_size == 0）
 *   SS_ONSTACK  — sp 当前位于备用栈内
 *   0           — 备用栈已设置，但 sp 不在其范围内
 */
int altstack_flags(unsigned long sp)
{
    pcb_t pcb = ttosProcessSelf();

    if (!pcb->altstack_size)
    {
        return SS_DISABLE;
    }

    return altstack_on_stack(sp) ? SS_ONSTACK : 0;
}

/**
 * kernel_sigaltstack - sigaltstack 系统调用核心实现
 * @ss:          新备用栈描述符（NULL 表示仅查询）
 * @oss:         用于输出旧备用栈描述符（NULL 表示不需要）
 * @sp:          当前用户栈指针（用于判断是否已在备用栈上）
 * @min_ss_size: 备用栈最小字节数（通常为 MINSIGSTKSZ）
 *
 * 返回 0 表示成功；
 * -EPERM  — 当前已在备用栈上，禁止修改；
 * -EINVAL — ss_flags 包含非法位；
 * -ENOMEM — 新栈大小小于 min_ss_size。
 */
int kernel_sigaltstack(const stack_t *ss, stack_t *oss, unsigned long sp, size_t min_ss_size)
{
    pcb_t pcb = ttosProcessSelf();
    int ret = 0;

    /* 输出旧备用栈信息：合并运行时状态（SS_ONSTACK）与持久标志（SS_FLAG_BITS） */
    if (oss)
    {
        memset(oss, 0, sizeof(stack_t));

        oss->ss_sp    = (void __user *)pcb->altstack_base;
        oss->ss_size  = pcb->altstack_size;
        oss->ss_flags = altstack_flags(sp) | (pcb->altstack_flags & SS_FLAG_BITS);
    }

    if (ss)
    {
        void __user *ss_sp   = ss->ss_sp;
        size_t       ss_size = ss->ss_size;
        unsigned     ss_flags = ss->ss_flags;
        int          ss_mode;

        /* 正在备用栈上运行时不允许修改 */
        if (unlikely(altstack_on_stack(sp)))
        {
            return -EPERM;
        }

        /* ss_mode 只允许 SS_DISABLE、SS_ONSTACK（忽略，历史兼容）或 0 */
        ss_mode = ss_flags & ~SS_FLAG_BITS;
        if (unlikely(ss_mode != SS_DISABLE && ss_mode != SS_ONSTACK && ss_mode != 0))
        {
            return -EINVAL;
        }

        /* 参数与当前状态完全相同，提前返回，避免不必要的修改 */
        if (pcb->altstack_base == (unsigned long)ss_sp &&
            pcb->altstack_size == ss_size &&
            pcb->altstack_flags == ss_flags)
        {
            return 0;
        }

        if (ss_mode == SS_DISABLE)
        {
            /* 禁用备用栈：清零尺寸和地址 */
            ss_size = 0;
            ss_sp   = NULL;
        }
        else
        {
            /* 启用备用栈：检查最小尺寸 */
            if (unlikely(ss_size < min_ss_size))
                ret = -ENOMEM;
        }

        if (!ret)
        {
            pcb->altstack_base    = (unsigned long)ss_sp;
            pcb->altstack_size  = ss_size;
            pcb->altstack_flags = ss_flags;
        }
    }

    return ret;
}

/**
 * altstack_restore_from_user - 从用户空间恢复备用栈配置（信号返回时调用）
 * @uss: 用户空间 stack_t 指针
 * @sp:  当前用户栈指针
 */
int altstack_restore_from_user(const stack_t __user *uss, unsigned long sp)
{
    stack_t new;

    if (copy_from_user(&new, uss, sizeof(stack_t)))
    {
        return -EFAULT;
    }

    (void)kernel_sigaltstack(&new, NULL, sp, MINSIGSTKSZ);

    /* 忽略 EFAULT 以外的错误（POSIX 信号返回路径不向用户报告此错误） */
    return 0;
}

/**
 * altstack_save_to_user - 将当前备用栈配置写入用户空间（构建信号帧时调用）
 * @uss: 用户空间 stack_t 指针
 * @sp:  当前用户栈指针（未使用，保留与 altstack_restore_from_user 对称的接口）
 */
void altstack_save_to_user(stack_t __user *uss, unsigned long sp)
{
    pcb_t pcb = ttosProcessSelf();

    uss->ss_sp    = (void *)pcb->altstack_base;
    uss->ss_flags = pcb->altstack_flags;
    uss->ss_size  = pcb->altstack_size;
}

/**
 * altstack_reset - 重置备用栈为禁用状态（进程退出或 exec 时调用）
 * @pcb: 目标线程控制块
 */
void altstack_reset(pcb_t pcb)
{
    pcb->altstack_base  = 0;
    pcb->altstack_size  = 0;
    pcb->altstack_flags = SS_DISABLE;
}
