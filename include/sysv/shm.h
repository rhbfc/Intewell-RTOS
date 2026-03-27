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

#ifndef __SYS_V_SHM_H
#define __SYS_V_SHM_H
#include <sys/shm.h>
#include <sys/ipc.h>
#include <system/types.h>

int sysv_shmget (key_t key, size_t size, int shmflg);
unsigned long sysv_shmat (int shmid, virt_addr_t shmaddr, int shmflg);
int sysv_shmdt (virt_addr_t shmaddr);
int sysv_shmctl (int shmid, int cmd, struct shmid_ds *buf);

#endif // __SYS_V_SHM_H
