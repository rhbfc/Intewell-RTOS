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

/*
 * @file： cmd_task.c
 * @brief：
 *       <li>task命令的实现，显示任务信息，主要包括任务ID、任务名、任务优先级、任务状态、任务入口函数、任务等待信息，任务tick数。</li>
 */

/************************头 文 件******************************/
#include <shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <ttos.h>
#include <ttosProcess.h>
/************************宏 定 义******************************/

#define TASK_READY "R"
#define TASK_RUN "U"
#define TASK_SUSPEND "S"
#define TASK_WAIT "W"
#define TASK_DORMANT "D"
#define TASK_FIRST "F"

#define TASK_UNKNOWN "-"

#define STAT_SPLIT "|"

/************************类型定义*****************************/

/************************外部声明*****************************/

/************************前向声明*****************************/

/************************模块变量*****************************/

/************************全局变量*****************************/

/************************实   现*******************************/

/* taks任务状态转换 */
char *task_state_to_char(T_UWORD option)
{
    T_UWORD cmp_stt = 0; // 状态复合标识
    static char stt[64]; // 状态转换结果

#define WAIT_STAT                                                                                  \
    (TTOS_TASK_DELAYING | TTOS_TASK_PWAITING | TTOS_TASK_CWAITING | TTOS_TASK_WAITING_FOR_EVENT |  \
     TTOS_TASK_WAITING_FOR_MSGQ | TTOS_TASK_WAITING_FOR_SEMAPHORE |                                \
     TTOS_TASK_WAITING_FOR_JOIN_AT_EXIT | TTOS_TASK_WAITING_FOR_SIGNAL |                           \
     TTOS_TASK_WAITING_FOR_MUTEX | TTOS_TASK_WAITING_FOR_RWLOCK |                                  \
     TTOS_TASK_WAITING_FOR_TASK_EXIT | TTOS_TASK_WAITING_FOR_FUTEX |                               \
     TTOS_TASK_WAITING_FOR_PROCESS | TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL)

    memset(stt, 0, sizeof(stt));

    if (option & (TTOS_TASK_DORMANT | TTOS_TASK_READY | TTOS_TASK_RUNNING | TTOS_TASK_SUSPEND |
                  WAIT_STAT | TTOS_TASK_FIRSTRUN))
    {
        if (option & TTOS_TASK_FIRSTRUN)
        {
            cmp_stt = 1;
            strcat(stt, TASK_FIRST);
        }

        if (option & TTOS_TASK_DORMANT)
        {
            if (cmp_stt != 0)
            {
                strcat(stt, STAT_SPLIT);
            }
            cmp_stt = 1;
            strcat(stt, TASK_DORMANT);
        }
        if (option & TTOS_TASK_READY)
        {
            if (cmp_stt != 0)
            {
                strcat(stt, STAT_SPLIT);
            }
            cmp_stt = 1;
            strcat(stt, TASK_READY);
        }
        if (option & TTOS_TASK_RUNNING)
        {
            if (cmp_stt != 0)
            {
                strcat(stt, STAT_SPLIT);
            }
            cmp_stt = 1;
            strcat(stt, TASK_RUN);
        }
        if (option & TTOS_TASK_SUSPEND)
        {
            if (cmp_stt != 0)
            {
                strcat(stt, STAT_SPLIT);
            }
            cmp_stt = 1;
            strcat(stt, TASK_SUSPEND);
        }
        if (option & WAIT_STAT)
        {
            if (cmp_stt != 0)
            {
                strcat(stt, STAT_SPLIT);
            }
            strcat(stt, TASK_WAIT);
        }
    }
    else
    {
        strcat(stt, TASK_UNKNOWN);
    }

    return stt;
}

static int is_hex(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return true;
    }

    if (ch >= 'a' && ch <= 'f')
    {
        return true;
    }

    if (ch >= 'A' && ch <= 'F')
    {
        return true;
    }

    return false;
}

static int string_all_digit_hex(char *string)
{
    int len = strlen(string);
    int n = 0;

    for (n = 0; n < len; n++)
    {
        if (!is_hex(string[n]))
        {
            return false;
        }
    }

    return true;
}
T_UBYTE corePriorityToPthread(int priority);

static char *task_policy_string(int policy)
{
    switch (policy)
    {
    case SCHED_OTHER:
    case SCHED_FIFO:
        return "FIFO";
    case SCHED_RR:
        return "RR";
    default:
        return "ERR";
    }
}

/**
 * @brief:
 *      shell
 * task命令处理handler，显示任务信息，主要包括任务ID、任务名、任务优先级、任务状态、任务入口函数、任务等待信息，任务tick数。
 *  终端的作用
 * @param[in]: shell： 传入的对应shell终端的实例
 * @param[in]: argc： 参数个数
 * @param[in]: argv： 参数指针数组
 * @return:
 *  命令执行是否成功：0:成功 -1:失败
 */
int shell_main_task(size_t argc, char **argv)
{
    T_UWORD TaskIdCount = 0;
    TASK_ID *TaskId = NULL;
    T_TTOS_TaskInfo TaskInfo = {0};
    int *is_taskid_find = NULL;
    int n = 0;
    int i = 1;
    int special_task_flag = false;
    int task_cnt = 0;
    unsigned long val = 0;
    char *endptr = 0;
    int ret = -1;
    int title_show = false;
    int *error_arg = NULL;
    int error_arg_show = false;
    char aff_cpu[8], run_cpu[8];

    if (2 <= argc)
    {
        special_task_flag = true;
    }

    ret = TTOS_GetTaskCount(&TaskIdCount);
    if (TTOS_OK != ret)
    {
        printk(" %s %d TTOS_GetTaskCountfaile ret: %d\n", __FILE__, __LINE__, ret);
        return ret;
    }

    TaskId = (TASK_ID *)malloc(TaskIdCount * sizeof(TASK_ID));
    if (NULL == TaskId)
    {
        return -1;
    }

    ret = TTOS_GetTaskIdList(TaskId, TaskIdCount);
    if (TTOS_OK != ret)
    {
        printk(" %s %d TTOS_GetTaskIdList failed ret: %d\n", __FILE__, __LINE__, ret);
        goto exit;
    }

    if (true == special_task_flag)
    {
        error_arg = (int *)malloc(sizeof(int) * argc);
        if (NULL == error_arg)
        {
            ret = -1;
            goto exit;
        }
        memset(error_arg, true, sizeof(int) * argc);

        is_taskid_find = (int *)malloc(sizeof(int) * argc);
        if (NULL == is_taskid_find)
        {
            ret = -1;
            goto exit;
        }
        memset(is_taskid_find, false, sizeof(int) * argc);

        for (i = 1; i < argc; i++)
        {
            for (n = 0; n < TaskIdCount; n++)
            {
                /* 对输入值进行检查 */
                if ((strstr(argv[i], "0X") != NULL) || (strstr(argv[i], "0x") != NULL))
                {
                    /* 检查16进制输入，是否真的全是16进制 */
                    if (!string_all_digit_hex(argv[i] + 2))
                    {
                        is_taskid_find[i] = true;
                        error_arg[i] = false;
                        ret = -1;
                        break;
                    }

                    /* 字符转16进制数字 */
                    val = strtoul(argv[i], &endptr, 16);

                    if (val == (unsigned long)TaskId[n])
                    {
                        ret = TTOS_GetTaskInfo(TaskId[n], &TaskInfo);
                        if (TTOS_OK != ret)
                        {
                            printk(" %s %d TTOS_GetTaskInfo failed ret: %d\n", __FILE__, __LINE__,
                                   ret);

                            ret = -1;
                            goto exit;
                        }
                        if (false == title_show)
                        {
                            title_show = true;
                            printk("\n   PID TASK: ID            PRIOR  AFF-CPU "
                                   " RUN-CPU           STATE  ENTRY           "
                                   "     WAIT      TICKS  NAME\n");
                        }

                        if (-1 == TaskInfo.affinityCpuIndex)
                        {
                            memcpy(aff_cpu, "    ANY", 8);
                        }
                        else
                        {
                            snprintf(aff_cpu, 8, "%7d", TaskInfo.affinityCpuIndex);
                        }

                        if (-1 == TaskInfo.cpuIndex)
                        {
                            memcpy(run_cpu, "   None", 8);
                        }
                        else
                        {
                            snprintf(run_cpu, 8, "%7d", TaskInfo.cpuIndex);
                        }

                        printk(" %5d %-18p  %5d  %7s  %7s    %12s  %-18p  %5d "
                               "%10d  %-32s\n",
                               TaskId[n]->ppcb ? get_process_pid((pcb_t)TaskId[n]->ppcb) : -1,
                               (void *)TaskInfo.id, corePriorityToPthread(TaskInfo.taskPriority),
                               aff_cpu, run_cpu, task_state_to_char(TaskInfo.state), TaskInfo.entry,
                               TaskInfo.option, (int)(TaskInfo.executedTicks), TaskInfo.name);

                        is_taskid_find[i] = true;
                        error_arg[i] = true;
                        task_cnt++;
                    }
                }
                else
                {
                    is_taskid_find[i] = true;
                    error_arg[i] = false;
                    ret = -1;
                    break;
                }
            }
        }

        if (0 != task_cnt)
            printk(" TOTAL MATCH: %d\n", task_cnt);

        for (i = 1; i < argc; i++)
        {
            if (false == is_taskid_find[i])
            {
                printk(" TASK:ID %s not found\n", argv[i]);
            }
        }

        for (i = 1; i < argc; i++)
        {
            if (false == error_arg[i])
            {
                if (false == error_arg_show)
                {
                    error_arg_show = true;
                    printk(" Invalid Args: TASK ID must be hexadecimal with "
                           "prefix like 0X or 0x\n");
                }
                printk(" Invalid TASK:ID %s\n", argv[i]);
                ;
            }
        }
    }
    else
    {
        printk("   TID TASK: ID            PRIOR       AFF-CPU   RUN-CPU            "
               "STATE          ENTRY                 WAIT               TICKS    NAME\n");

        for (n = 0; n < TaskIdCount; n++)
        {
            ret = TTOS_GetTaskInfo(TaskId[n], &TaskInfo);
            if (TTOS_OK != ret)
            {
                printk(" %s %d TTOS_GetTaskInfo failed ret: %d\n", __FILE__, __LINE__, ret);
                ret = -1;
                goto exit;
            }

            if (-1 == TaskInfo.affinityCpuIndex)
            {
                memcpy(aff_cpu, "    ANY", 8);
            }
            else
            {
                snprintf(aff_cpu, 8, "%7d", TaskInfo.affinityCpuIndex);
            }

            if (-1 == TaskInfo.cpuIndex)
            {
                memcpy(run_cpu, "   None", 8);
            }
            else
            {
                snprintf(run_cpu, 8, "%7d", TaskInfo.cpuIndex);
            }

            printk(" %5d %-18p  %3d(%s) %7s  %s %12s(0x%08X)  %-18p  %-18p %10d  %-32s\r",
                   TaskId[n]->tid, (void *)TaskInfo.id,
                   corePriorityToPthread(TaskInfo.taskPriority),
                   task_policy_string(TaskId[n]->taskSchedPolicy), aff_cpu, run_cpu,
                   task_state_to_char(TaskInfo.state), TaskInfo.state, TaskInfo.entry,
                   TaskId[n]->wait.id, (int)(TaskInfo.executedTicks), TaskInfo.name);

            task_cnt++;
        }

        printk(" TOTAL : %d\n", task_cnt);
    }

exit:
    if (NULL != TaskId)
    {
        free(TaskId);
        TaskId = NULL;
    }

    if (NULL != error_arg)
    {
        free(error_arg);
        error_arg = NULL;
    }

    if (NULL != is_taskid_find)
    {
        free(is_taskid_find);
        is_taskid_find = NULL;
    }

    return ret;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 ts, shell_main_task, show task info);
