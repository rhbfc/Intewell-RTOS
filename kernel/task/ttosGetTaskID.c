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

/* @<MODULE */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定任务的ID。
 * @param[in]: taskName: 任务名字，当为NULL时表示获取当前任务的ID
 * @param[out]: taskID: 任务ID
 * @return:
 *    TTOS_INVALID_NAME：<taskName>指定的任务不存在。
 *    TTOS_INVALID_ADDRESS：<taskID>为NULL。
 *    TTOS_OK：获取任务ID成功。
 * @notes:
 *    由于DeltaTT允许同名任务存在，当用户配置了多个同名任务时，调用本接口
 *    获取任务ID时只会获取到配置表中第一个与指定名字匹配的任务ID，可能出现和预期不一致的情况。
 * @implements: RTE_DTASK.15.1
 */
T_TTOS_ReturnCode TTOS_GetTaskID (T_UBYTE *taskName, TASK_ID *taskID)
{
    T_TTOS_ObjectCoreID returnObjectCoreID;

    /* @REPLACE_BRACKET: <taskID>为空 */
    if (NULL == taskID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskName>为空 */
    if (NULL == taskName)
    {
        /* @KEEP_COMMENT:
         * 获取当前运行任务ttosManager.runningTask的ID号存放至<taskID> */
        *taskID = ttosGetRunningTask ()->taskControlId;
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    returnObjectCoreID = ttosGetIdByName (taskName, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: NULL == returnObjectCoreID */
    if (NULL == returnObjectCoreID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NAME */
        return (TTOS_INVALID_NAME);
    }

    *taskID = (TASK_ID)returnObjectCoreID;
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
