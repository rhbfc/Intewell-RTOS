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

/* @<MOD_INFO   */

/*
 * @file: DC_RT_3.pdl
 * @brief:
 *       <li>本模块对信号量进行的各种管理。</li>
 * @implements: RT.3
 */
/* @MOD_INFO> */

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
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定信号量的ID。
 * @param[in]: semaName: 信号量的名字
 * @param[out]: semaID: 信号量的ID
 * @return:
 *    TTOS_INVALID_ADDRESS：<semaName>或<semaID>为NULL。
 *    TTOS_INVALID_NAME：<semaName>指定的信号量不存在。
 *    TTOS_OK：获取信号量的ID成功。
 * @notes:
 *    由于DeltaTT允许同名信号量存在，当用户配置了多个同名信号量时，调用本接口获取信号量ID
 *    时只会获取到配置表中第一个与指定名字匹配的信号量ID，可能出现和预期不一致的情况。
 * @implements: DT.3.1
 */
T_TTOS_ReturnCode TTOS_GetSemaID (T_UBYTE *semaName, SEMA_ID *semaID)
{
    T_TTOS_ObjectCoreID returnObjectCoreID;

    /* @REPLACE_BRACKET: <semaName>为NULL || <semaID>为NULL */
    if ((NULL == semaName) || (NULL == semaID))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    returnObjectCoreID = ttosGetIdByName (semaName, TTOS_OBJECT_SEMA);

    if (NULL == returnObjectCoreID)
    {
        return (TTOS_INVALID_NAME);
    }

    *semaID = (SEMA_ID)returnObjectCoreID;

    return (TTOS_OK);
}
