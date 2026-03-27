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

/* ttos控制结构 */
T_EXTERN T_TTOS_TTOSManager ttosManager;

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实    现******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定定时器的ID。
 * @param[in]: timerName: 定时器名字
 * @param[out]: timerID: 定时器的ID
 * @return:
 *    TTOS_INVALID_ADDRESS：<timeName>为NULL或者<timeID>为NULL。
 *    TTOS_INVALID_NAME：<timerName>指定的定时器不存在。
 *    TTOS_OK：成功获取定时器的ID。
 * @notes:
 *    由于DeltaTT允许同名定时器存在，当用户配置了多个同名定时器时，调用本接口获取定时
 *    器ID时只会获取到配置表中第一个与指定名字匹配的定时器ID，可能出现和预期不一致
 *    的情况。
 * @implements: DT.4.2
 */
T_TTOS_ReturnCode TTOS_GetTimerID (T_UBYTE *timerName, TIMER_ID *timerID)
{
    T_TTOS_ObjectCoreID returnObjectCoreID;

    /* @REPLACE_BRACKET: <timerName>为NULL || <timerID>为NULL */
    if ((NULL == timerName) || (NULL == timerID))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    returnObjectCoreID = ttosGetIdByName (timerName, TTOS_OBJECT_TIMER);

    if (NULL == returnObjectCoreID)
    {
        return (TTOS_INVALID_NAME);
    }

    *timerID = (TIMER_ID)returnObjectCoreID;

    return (TTOS_OK);
}
