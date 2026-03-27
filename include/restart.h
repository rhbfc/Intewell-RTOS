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

#ifndef __RESTART_H
#define __RESTART_H

#include <time.h>
#include <stdint.h>

struct T_TTOS_ProcessControlBlock;
typedef struct T_TTOS_ProcessControlBlock *pcb_t;

#ifndef __user
#define __user
#endif

#define ERR_RESTART_IF_SIGNAL     600   /* 如果被信号打断，并且信号允许重启（取决于 SA_RESTART），则重启 syscall */
#define ERR_RESTART_ALWAYS        601   /* 无论是否有信号，都自动重启（不会返回 EINTR） */
#define ERR_RESTART_IF_NO_HANDLER 602   /* 如果信号没有 handler（默认处理），才重启。主要针对SIGSTOP/SIGCONT,在nanosleep一类函数使用 绝对 时间休眠,直接重启系统调用,能继续重启休眠 */
#define ERR_RESTART_WITH_BLOCK    603	/* 使用 restart_block 机制重启（比如 nanosleep 这种需要恢复上下文的 syscall）。主要针对SIGSTOP/SIGCONT,在nanosleep一类函数使用 相对 时间休眠, restart_block+restart_syscall,能继续重启休眠 */

enum rmtp_exist_type {
	RMTP_NONE	    = 0,
	RMTP_PRESENT	= 1,
};

enum rmtp_timespec_type
{
	TIMESPEC,
	TIMESPEC32,
	TIMESPEC64
};

struct restart_info
{
	long (*restart_func)(struct restart_info *);
	union
	{
		struct
		{
			clockid_t clockid;
			enum rmtp_exist_type rmtp_exist;
			void *rmtp;
			enum rmtp_timespec_type rmtp_timespec_type;
			uint64_t expires;
		} nanosleep;
	};
};

void restart_init (pcb_t pcb, long (*restart_func)(struct restart_info *), void *rmtp);
void restart_init_nanosleep(pcb_t pcb,
			    long (*restart_func)(struct restart_info *),
			    void *rmtp,
			    enum rmtp_timespec_type rmtp_timespec_type);
long restart_set_func(struct restart_info *restart,
					long (*restart_func)(struct restart_info *));
long restart_return_eintr(struct restart_info *param);

#endif /* __RESTART_H */
