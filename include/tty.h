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

#ifndef __TTY_H__
#define __TTY_H__

#include <termios.h>

struct inode;
typedef struct inode * tty_t;
typedef struct T_TTOS_ProcessControlBlock      *pcb_t;


int tty_clear_ctty(pcb_t pcb);
int tty_set_ctty(tty_t tty, pcb_t pcb);
int tty_check_special(tcflag_t tc_lflag, const char *buf, size_t size);

#endif /* __TTY_H__ */

