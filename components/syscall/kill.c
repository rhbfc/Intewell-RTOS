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
#include <errno.h>
#include <fs/fs.h>
#include <pgroup.h>
#include <process_signal.h>
#include <signal.h>
#include <stdio.h>
#include <tasklist_lock.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 向进程发送信号
 *
 * 该函数实现了一个系统调用，用于向指定进程发送信号。
 *
 * @param[in] pid 目标进程ID：
 *                - >0：发送给指定进程
 *                - =0：发送给同进程组的所有进程
 *                - =-1：发送给所有可发送的进程
 *                - <-1：发送给进程组|pid|中的所有进程
 * @param[in] sig 要发送的信号
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功发送信号。
 * @retval -EINVAL sig无效。
 * @retval -ESRCH 目标进程或进程组不存在。
 * @retval -EPERM 没有权限发送信号。
 *
 * @note 1. 常用信号包括SIGTERM、SIGKILL等。
 *       2. 部分信号不能被忽略（如SIGKILL）。
 *       3. 权限检查基于进程的实际或有效用户ID。
 *       4. 进程可以向自己发送信号。
 */
/**
 * @brief 系统调用实现：向进程发送信号。
 *
 * 该函数实现了一个系统调用，用于向指定进程发送信号。
 *
 * @param[in] pid 目标进程ID：
*    If pid is positive, then signal sig is sent to the process with the ID specified by pid.
*    If pid equals 0, then sig is sent to every process in the process group of the calling process.
*    If pid equals -1, then sig is sent to every process for which the calling process has permission to send signals, except for process 1 (init), but see below.
*    If pid is less than -1, then sig is sent to every process in the process group whose ID is -pid.
*    If sig is 0, then no signal is sent, but existence and permission checks are still performed; this can be used to check for the existence of a process ID or process group ID that the caller is permitted to signal.
 * @param[in] sig 要发送的信号
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功发送信号。
 * @retval -EINVAL sig无效。
 * @retval -ESRCH 目标进程或进程组不存在。
 * @retval -EPERM 没有权限发送信号。
 *
 * @note 1. 常用信号包括SIGTERM、SIGKILL等。
 *       2. 部分信号不能被忽略（如SIGKILL）。
 *       3. 权限检查基于进程的实际或有效用户ID。
 *       4. 进程可以向自己发送信号。
 */
DEFINE_SYSCALL(kill, (pid_t pid, int sig))
{
    long flags = 0;
    int ret = 0;
    pcb_t pcb;

    /* 发送给指定进程 */
    if (pid > 0)
    {
        ret = kernel_signal_send(pid, TO_PROCESS, sig, SI_USER, 0);
    }
    else if (pid < -1 || pid == 0)/* pid < -1:指定进程组 pid == 0:当前进程组 */
    {
        pid_t pgid = 0;
        pgroup_t group;

        /* 当前进程组 */
        if (pid == 0)
        {
            pgid = process_pgid_get_byprocess(ttosProcessSelf());
        }
        else
        {
            /* 指定进程组 */
            pgid = -pid;
        }

        ret = kernel_signal_send(pgid, TO_PGROUP, sig, SI_USER, 0);
    }
    else if (pid == -1)/* 发送给除了pid为1之外，所有有权限发送的进程 */
    {
        /* @KEEP_COMMENT: 禁止调度器 */
        tasklist_lock();

        process_foreach((void (*)(pcb_t, void *))process_signal_kill_to, (void *)(long)sig);

        /* @KEEP_COMMENT: 重新使能调度 */
        tasklist_unlock();
        ret = 0;
    }

    return ret;
}

#ifdef CONFIG_SHELL
#include <shell.h>

int kill_cmd(int argc, const char *argv[])
{
    if (argc != 2)
    {
        printf("argc should be 2");
    }
    pid_t pid = atoi(argv[1]);
    if (pid == 0)
    {
        printf("Can not kill kernel");
    }
    return SYSCALL_FUNC(kill)(pid, 0);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 kill, kill_cmd, kill process);
#endif /* CONFIG_SHELL */
