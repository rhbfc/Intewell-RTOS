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
#include <uaccess.h>
#include <period_sched_group.h>

/**
 * @brief 系统调用实现：激活周期线程。
 *
 * 该函数实现了一个系统调用，用于激活周期线程。
 *
 * @param period 周期时间。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
int syscall_period_thread_active (int period)
{
	if (0 == period)
	{
		/* �����������ڵ��������� */
		return period_thread_active_all ();
	}
	else
	{
		/* ����ָ�����ڵ��������� */
		return period_thread_active_group (period);
	}
}