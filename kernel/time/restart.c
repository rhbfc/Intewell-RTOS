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

#include <ttosProcess.h>
#include <restart.h>
#include <errno.h>

long restart_return_eintr(struct restart_info *param)
{
	return -EINTR;
}

long restart_set_func(struct restart_info *restart,
					long (*restart_func)(struct restart_info *))
{
	restart->restart_func = restart_func;
	return -ERR_RESTART_WITH_BLOCK;
}

void restart_init_nanosleep(pcb_t pcb,
			    long (*restart_func)(struct restart_info *),
			    void *rmtp,
			    enum rmtp_timespec_type rmtp_timespec_type)
{
	pcb->restart.restart_func = restart_func;
	pcb->restart.nanosleep.rmtp_exist = rmtp ? RMTP_PRESENT : RMTP_NONE;
	pcb->restart.nanosleep.rmtp = rmtp;
	pcb->restart.nanosleep.rmtp_timespec_type = rmtp_timespec_type;
}

void restart_init (pcb_t pcb, long (*restart_func)(struct restart_info *), void *rmtp)
{
	restart_init_nanosleep(pcb, restart_func, rmtp, TIMESPEC);
}
