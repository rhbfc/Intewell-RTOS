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
#include <time/ktime.h>
#include <assert.h>
#include <uaccess.h>
#include <time.h>
#include <time/ktimer.h>
#include <process_signal.h>
#include <system/macros.h>

/**
 * @brief 系统调用实现：设置进程的间隔定时器。
 *
 * 该函数实现了一个系统调用，用于设置和管理进程的间隔定时器。
 * 定时器可以在指定的时间间隔发送SIGALRM（ITIMER_REAL）、
 * SIGVTALRM（ITIMER_VIRTUAL）或SIGPROF（ITIMER_PROF）信号。
 *
 * @param[in] which 定时器类型：
 *                - ITIMER_REAL: 实时定时器，计算实际流逝的时间
 *                - ITIMER_VIRTUAL: 虚拟定时器，只计算进程用户态时间
 *                - ITIMER_PROF: 性能分析定时器，计算用户态和内核态时间
 * @param[in] value 新的定时器值：
 *                    - it_interval: 定时器间隔时间
 *                    - it_value: 距离下次到期的时间
 * @param[out] ovalue 原定时器值：
 *                     - NULL: 不获取原值
 *                     - 非NULL: 返回原定时器值
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT value或ovalue指向无效内存。
 * @retval -EINVAL which无效或定时器值无效。
 *
 * @note 1. 如果it_value为0，定时器将被禁用。
 *       2. 如果it_interval为0，定时器将只触发一次。
 *       3. ITIMER_VIRTUAL和ITIMER_PROF只在进程运行时计时。
 */
DEFINE_SYSCALL(setitimer, (int which, struct itimerval __user *value,
    struct itimerval __user *ovalue))
{
	struct itimerspec64 set_buffer, get_buffer;
	int error;

	if (value) 
	{
		error = copy_user_itimerval_to_itimerspec64(&set_buffer, value);
		if (error)
        {
			return error;
        }
	} 
	else 
	{
		memset(&set_buffer, 0, sizeof(set_buffer));
	}

	error = apply_itimer(which, &set_buffer, ovalue ? &get_buffer : NULL);
	if (error || !ovalue)
    {
		return error;
    }

	if (copy_itimerspec64_to_user_itimerval(ovalue, &get_buffer))
    {
		return -EFAULT;
    }

	return 0;
}
