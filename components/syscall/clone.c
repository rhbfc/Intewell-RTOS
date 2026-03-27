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

#include "syscall_internal.h"
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：创建一个新进程。
 *
 * 该函数实现了一个系统调用，用于创建一个新进程。
 *
 * @param[in] clone_flags 进程标志。可以是以下标志的组合：
 *              - CLONE_VM：父进程和子进程共享内存空间。
 *              - CLONE_FS：父进程和子进程共享文件系统信息。
 *              - CLONE_FILES：父进程和子进程共享文件描述符表。
 *              - CLONE_SIGHAND：父进程和子进程共享信号处理程序表。
 *              - CLONE_PTRACE：如果父进程正在被跟踪，则子进程也被跟踪。
 *              - CLONE_VFORK：父进程挂起，直到子进程释放虚拟内存资源。
 *              - CLONE_PARENT：子进程的父进程与调用进程相同。
 *              - CLONE_THREAD：子进程与调用进程在同一个线程组中。
 *              - CLONE_NEWNS：子进程在一个新的挂载命名空间中启动。
 *              - CLONE_SYSVSEM：父进程和子进程共享一个 System V 信号量调整列表。
 *              - CLONE_SETTLS：设置线程局部存储。
 *              - CLONE_PARENT_SETTID：在父进程的内存中存储子进程的线程 ID。
 *              - CLONE_CHILD_SETTID：在子进程的内存中存储子进程的线程 ID。
 *              - CLONE_CHILD_CLEARTID：子进程退出时清除子进程的线程 ID。
 *              - CLONE_DETACHED：子进程退出时不向父进程发送信号。
 *              - CLONE_UNTRACED：跟踪进程不能强制 CLONE_PTRACE 子进程。
 *              - CLONE_STOPPED：子进程初始时被停止。
 *              - CLONE_IO：新进程与调用进程共享 I/O 上下文。
 *              - CLONE_NEWCGROUP：在新的 cgroup 命名空间中创建进程。
 *              - CLONE_NEWIPC：在新的 IPC 命名空间中创建进程。
 *              - CLONE_NEWNET：在新的网络命名空间中创建进程。
 *              - CLONE_NEWPID：在新的 PID 命名空间中创建进程。
 *              - CLONE_NEWUSER：在新的用户命名空间中创建进程。
 *              - CLONE_NEWUTS：在新的 UTS 命名空间中创建进程。
 *              - CLONE_PID：创建的子进程与调用进程具有相同的进程 ID。
 *              - CLONE_PIDFD：分配一个指向子进程的 PID 的文件描述符。
 *              - CLONE_CLEAR_SIGHAND：子线程的信号处理程序被重置为默认值。
 *              - CLONE_INTO_CGROUP：将子进程创建在不同的 cgroup 中。
 * 
 * @param[in] newsp 新进程的栈指针。
 * @param[in] set_child_tid 子进程的线程 ID。
 * @param[in] tls 线程局部存储。
 * @param[in] clear_child_tid 清除子进程的线程 ID。
 * @return 成功时返回子进程的进程 ID，失败时返回负值错误码。
 * @retval -ENOMEM 内核内存不足。
 * @retval -EINVAL clone_flags 错误。
 * @retval -EAGAIN 进程过多。
 * @retval -EPERM 权限不足。
 * @retval -EUSERS 用户命名空间嵌套过多。
 * @retval -ENOSPC PID 命名空间嵌套过多。
 * @retval -EOPNOTSUPP CLONE_INTO_CGROUP 指定的 cgroup 文件描述符引用了一个无效状态的域。
 * @retval -EBUSY CLONE_INTO_CGROUP 指定的 cl_args.flags 中包含 CLONE_INTO_CGROUP，但 cl_args.cgroup 引用了一个启用了域控制器的版本 2 cgroup。
 * @retval -EINVAL 
 *              - flags 掩码中同时指定了 CLONE_SIGHAND 和 CLONE_CLEAR_SIGHAND。
 *              - flags 掩码中指定了 CLONE_SIGHAND，但未指定 CLONE_VM。
 *              - flags 掩码中指定了 CLONE_THREAD，但未指定 CLONE_SIGHAND。
 *              - flags 掩码中指定了 CLONE_THREAD，但当前进程之前使用 CLONE_NEWPID 标志调用了 unshare(2) 或使用 setns(2) 重新关联了自己到一个 PID 命名空间。
 *              - flags 掩码中同时指定了 CLONE_FS 和 CLONE_NEWNS。
 *              - flags 掩码中同时指定了 CLONE_NEWUSER 和 CLONE_FS。
 *              - flags 掩码中同时指定了 CLONE_NEWIPC 和 CLONE_SYSVSEM。
 *              - flags 掩码中指定了 CLONE_NEWPID 或 CLONE_NEWUSER 之一（或两者），以及 CLONE_THREAD 或 CLONE_PARENT 之一（或两者）。
 *              - 指定了 CLONE_PARENT，且调用进程是一个 init 进程。
 *              - flags 掩码中指定了 CLONE_NEWIPC，但内核未配置 CONFIG_SYSVIPC 和 CONFIG_IPC_NS 选项。
 *              - flags 掩码中指定了 CLONE_NEWNET，但内核未配置 CONFIG_NET_NS 选项。
 *              - flags 掩码中指定了 CLONE_NEWPID，但内核未配置 CONFIG_PID_NS 选项。
 *              - flags 掩码中指定了 CLONE_NEWUSER，但内核未配置 CONFIG_USER_NS 选项。
 *              - flags 掩码中指定了 CLONE_NEWUTS，但内核未配置 CONFIG_UTS_NS 选项。
 *              - stack 未对齐到适合此架构的边界。例如，在 aarch64 上，stack 必须是 16 的倍数。
 *              - clone() 中指定了 CLONE_PIDFD 与 CLONE_DETACHED。
 *              - flags 掩码中指定了 CLONE_PIDFD 与 CLONE_THREAD。
 *              - clone() 中指定了 CLONE_PIDFD 与 CLONE_PARENT_SETTID。
 *              -（仅限 AArch64，Linux 4.6 及更早版本）stack 未对齐到 126 位边界。
 * @retval -ENOMEM 无法为子进程分配足够的内存以分配任务结构，或复制需要复制的调用者上下文的部分。
 * @retval -ENOSPC flags 掩码中指定了 CLONE_NEWPID，但 PID 命名空间的嵌套深度限制将被超出；请参见 pid_namespaces(7)。
 * @retval -ENOSPC flags 掩码中指定了 CLONE_NEWUSER，并且调用将导致超出嵌套用户命名空间的数量限制。请参见 user_namespaces(7)。
 * @retval -ENOSPC flags 掩码中的一个值指定了创建新用户命名空间，但这样做将导致 /proc/sys/user 中相应文件中定义的限制被超出。有关详细信息，请参见 namespaces(7)。
 */
#ifdef CONFIG_CLONE_BACKWARDS
DEFINE_SYSCALL (clone, (unsigned long clone_flags, unsigned long newsp,
                        int __user *set_child_tid, unsigned long tls,
                        int __user *clear_child_tid))
#elif defined(CONFIG_CLONE_BACKWARDS2)
DEFINE_SYSCALL (clone, (unsigned long newsp, unsigned long clone_flags,
                        int __user *set_child_tid, int __user *clear_child_tid,
                        unsigned long tls))

#elif defined(CONFIG_CLONE_BACKWARDS3)
DEFINE_SYSCALL (clone, (unsigned long clone_flags, unsigned long newsp,
                        int stack_size, int __user *set_child_tid,
                        int __user *clear_child_tid, unsigned long tls))

#else
DEFINE_SYSCALL (clone, (unsigned long clone_flags, unsigned long newsp,
                        int __user *set_child_tid, int __user *clear_child_tid,
                        unsigned long tls))

#endif
{
    return do_fork (clone_flags, newsp, set_child_tid, clear_child_tid, tls,
                    NULL);
}
