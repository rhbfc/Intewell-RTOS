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

#ifndef __SIGCONTEXT_H
#define __SIGCONTEXT_H

#define ALGIN_BYTES 8

struct signal_ucontext
{
    /* 存放 信号handler执行完后，返回上一层函数调后，执行的指令(通常为两条指令,一条设置系统调用号，另外一条执行系统调用指令) */
    long sigreturn[4];
    siginfo_t si;
    __attribute__((aligned(ALGIN_BYTES)))
    ucontext_t frame;
};

#endif