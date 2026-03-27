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

#ifndef __SLEEP_H
#define __SLEEP_H

/* 可中断usleep */
unsigned long usleep_interruptible (unsigned useconds);

/* 可中断msleep */
unsigned long msleep_interruptible(unsigned int msecs);

/* 不可中断usleep */
int usleep (unsigned useconds);

/* 不可中断msleep */
void msleep (unsigned ms);

#endif