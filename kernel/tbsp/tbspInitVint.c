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

#define KLOG_TAG "Kernel"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
T_EXTERN T_VOID ttosSetMSRAdress (T_UWORD cpuID, T_ULONG msrAdress);
/************************模块变量******************************/
/************************全局变量******************************/

/* @<MOD_VAR */

#if CONFIG_SMP == 1
/* 虚拟中断嵌套层数 */
T_ULONG intNestLevel[CONFIG_MAX_CPUS];

/* 全局虚拟中断使能开关地址*/
T_ULONG ttosMSRAdress[CONFIG_MAX_CPUS];

/* 虚拟tick中断嵌套层数 ，当前只有主核才处理虚拟tick中断。*/
T_ULONG tbspTickVIntNestLevel[CONFIG_MAX_CPUS];
#else
/* 虚拟中断嵌套层数 */
T_ULONG intNestLevel = 0;

/* 全局虚拟中断使能开关地址*/
T_ULONG ttosMSRAdress;

T_ULONG tbspTickVIntNestLevel;
#endif /*CONFIG_SMP*/

/* @MOD_VAR> */

/************************实    现******************************/

/* @MODULE> */

/* @IGNORE_BEGIN: */
/*
 * @brief:
 *    获取当前CPU上的中断嵌套层数 。
 * @return:
 *    当前CPU上的中断嵌套层数 。
 */
T_ULONG ttosGetIntNestLevel (T_VOID)
{
#if CONFIG_SMP == 1
    TBSP_MSR_TYPE msr = 0U;
    T_ULONG       level;
    TBSP_GLOBALINT_DISABLE (msr);
    level = intNestLevel[cpuid_get ()];
    TBSP_GLOBALINT_ENABLE (msr);
    return (level);
#else
    return (intNestLevel);
#endif
}

/*
 * @brief:
 *    获取当前CPU上的tick中断嵌套层数 。
 * @return:
 *    当前CPU上的tick中断嵌套层数 。
 */
T_ULONG ttosGetTickIntNestLevel (T_VOID)
{
#if CONFIG_SMP == 1
    /*当前只有主核才会处理tick中断，所以不需要禁止中断来获取tick中断嵌套层数。*/
    return (tbspTickVIntNestLevel[cpuid_get ()]);
#else
    return (tbspTickVIntNestLevel);
#endif
}

/*
 * @brief:
 *    递增指定CPU上的tick中断嵌套层数。
 * @return:
 *    无。
 */
T_VOID ttosIncTickIntNestLevel (T_VOID)
{
#if CONFIG_SMP == 1
    tbspTickVIntNestLevel[cpuid_get ()]++;
#else
    tbspTickVIntNestLevel++;
#endif
}

/*
 * @brief:
 *    递减指定CPU上的tick中断嵌套层数。
 * @return:
 *    无。
 */
T_VOID ttosDecTickIntNestLevel (T_VOID)
{
#if CONFIG_SMP == 1
    tbspTickVIntNestLevel[cpuid_get ()]--;
#else
    tbspTickVIntNestLevel--;
#endif
}

/*
 * @brief:
 *    设置全局虚拟中断使能开关地址。
 * @param[in]: cpuID: 影响的cpuID
 * @param[in]: msrAdress: 全局虚拟中断使能开关地址
 * @return:
 *    无
 */
T_VOID ttosSetMSRAdress (T_UWORD cpuID, T_ULONG msrAdress)
{
#if CONFIG_SMP == 1
    ttosMSRAdress[cpuID] = msrAdress;
#else
    ttosMSRAdress = msrAdress;
#endif
}

/* @END_HERE: */
