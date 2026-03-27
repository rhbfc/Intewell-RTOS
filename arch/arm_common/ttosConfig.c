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

/************************头 文 件******************************/
#include <ttos.h>

/************************宏 定 义******************************/
#define RAW_SOCKET_TASK_PRIORITY 7
/* 静态配置的任务个数 */
#define CONFIG_TTOS_TASK_CONFIG_NUMBER 1

/* 用户可以创建的最大任务个数 */
#define CONFIG_TTOS_TASK_CREATE_MAX_NUMBER 1024

/* 静态配置的定时器个数 */
#define CONFIG_TTOS_TIMER_CONFIG_NUMBER 0

/* 用户可以创建的最大定时器个数 */
#define CONFIG_TTOS_TIMER_CREATE_MAX_NUMBER 1024

/* 静态配置的信号量个数 */
#define CONFIG_TTOS_SEMA_CONFIG_NUMBER 0

/* 用户可以创建的最大信号量个数 */
#define CONFIG_TTOS_SEMA_CREATE_MAX_NUMBER 1024

#ifdef TTOS_MSGQ
/* 静态配置的消息队列个数 */
#define CONFIG_TTOS_MSGQ_CONFIG_NUMBER 0

/* 静态配置一个消息队列中的消息个数 */
#define CONFIG_TTOS_MSG_NUMBER 10

/* 静态配置的消息长度 */
#define CONFIG_TTOS_MSG_SIZE 320

/* 用户可以创建的最大消息队列数量 */
#define CONFIG_TTOS_MSGQ_CREATE_MAX_NUMBER 1024
#endif

/*TTOS事件记录掩码*/
#define TTOS_RECORD_MASK (TTOS_RECORD_TASK_SWITCH_MASK)

/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/

/************************模块变量******************************/


T_MODULE T_TTOS_TaskControlBlock tCB[CONFIG_TTOS_TASK_CONFIG_NUMBER];

T_MODULE T_TTOS_ConfigTimer timerConfig[CONFIG_TTOS_TIMER_CONFIG_NUMBER];

T_MODULE T_TTOS_TimerControlBlock timerCB[CONFIG_TTOS_TIMER_CONFIG_NUMBER];

T_MODULE T_TTOS_ConfigSema semaConfig[CONFIG_TTOS_SEMA_CONFIG_NUMBER];

#ifdef TTOS_MSGQ
T_MODULE T_TTOS_ConfigMsgq msgqConfig[CONFIG_TTOS_MSGQ_CONFIG_NUMBER];
#endif

T_MODULE T_TTOS_SemaControlBlock semaCB[CONFIG_TTOS_SEMA_CONFIG_NUMBER];

#ifdef TTOS_MSGQ
T_MODULE T_TTOS_MsgqControlBlock msgqCB[CONFIG_TTOS_MSGQ_CONFIG_NUMBER];
#endif

/************************全局变量******************************/
T_TTOS_ConfigTable ttosConfigTable = {
    /* 任务对象的配置结构指针 */
    NULL,

    /* 任务的控制结构指针 */
    tCB,

    /*
     *所有任务的运行栈指针 ，根据任务运行栈的大小依次分配每个任务的
     *运行栈指针 ，此空间不能小于所有任务运行栈大小的总和
     */
    0,

    /* 配置的任务个数
       ，每个任务的栈大小总和不能大于nTaskStack定义的所有任务运行栈大小 */
    CONFIG_TTOS_TASK_CONFIG_NUMBER,

    /* 用户可以创建的最大任务个数 */
    CONFIG_TTOS_TASK_CREATE_MAX_NUMBER,

    /* 信号量对象的配置结构指针 */
    semaConfig,

    /* 信号量对象的控制结构指针 */
    semaCB,

    /* 配置的信号量个数 */
    CONFIG_TTOS_SEMA_CONFIG_NUMBER,

    /* 用户可以创建的信号量个数 */
    CONFIG_TTOS_SEMA_CREATE_MAX_NUMBER,

#ifdef TTOS_MSGQ
    /* 消息队列对象的配置结构指针 */
    msgqConfig,

    /* 消息队列对象的控制结构指针 */
    msgqCB,

    /* 静态配置的消息队列个数 */
    CONFIG_TTOS_MSGQ_CONFIG_NUMBER,

    /* 用户可以动态创建的消息队列个数 */
    CONFIG_TTOS_MSGQ_CREATE_MAX_NUMBER,
#endif

    /* 定时器对象的配置结构指针 */
    timerConfig,

    /* 定时器对象的控制结构指针 */
    timerCB,

    /* 配置的定时器个数 */
    CONFIG_TTOS_TIMER_CONFIG_NUMBER,

    /* 用户可以创建的定时器个数 */
    CONFIG_TTOS_TIMER_CREATE_MAX_NUMBER,

    /*
     *配置是否需要查询任务栈信息
     *，TRUE表示需要，DeltaTT初始化时会使用特殊的字符填充
     *栈空间，便于可以查询栈使用的情况；FALSE表示不需要
     */
    FALSE,

    /*
     *TTOS事件记录掩码
     *，配置DeltaTT运行过程中可记录的事件，现在可记录的事件为任务切换事件，
     *任务切换事件掩码为TTOS_RECORD_TASK_SWITCH_MASK
     */
    TTOS_RECORD_MASK,

};
