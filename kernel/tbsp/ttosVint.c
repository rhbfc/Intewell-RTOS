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

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosUtils.h>
#include <ttosUtils.inl>
#include <ttos_pic.h>

#define KLOG_TAG "Kernel"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/
/************************模块变量******************************/

/************************全局变量******************************/

/************************外部声明******************************/

/************************前向声明******************************/
void ttosSharedVintHandler (T_UWORD vintNum);
/************************实    现******************************/
/* @MODULE> */

/**
 * @brief:
 *    安装指定虚拟中断的用户处理程序。
 * @param[in]: vintType: 虚拟中断类型
 * @param[in]: vintNum: 虚拟中断号
 * @param[in]: handler: 虚拟中断入口函数
 * @param[in]: mode: 虚拟中断执行模式
 * @return:
 *    TTOS_INVALID_TYPE：<mode>所表示的虚拟中断执行模式不存在。
 *    TTOS_FAIL：<vintType>不是虚拟异常、虚拟tick中断、虚拟外部中断和虚拟服务中断；
 *               <vintType>是虚拟异常或虚拟外部中断或虚拟服务中断，并且<vintNum>不在虚拟中断号范围内。
 *    TTOS_OK：安装成功。
 * @implements: DT.6.4
 */
T_TTOS_ReturnCode TTOS_InstallVintHandler (T_VMK_VintType vintType,
                                           T_UWORD vintNum, T_VOID *handler,
                                           T_TTOS_VintMode mode)
{
    T_WORD ttosRet;

    ttosRet = ttos_pic_irq_install (vintNum, handler, NULL, 0, NULL);
    /* @REPLACE_BRACKET: ret */
    return (ttosRet);
}

/**
 * @brief:
 *    卸载指定虚拟中断的用户处理程序。
 * @param[in]: vintType: 虚拟中断类型
 * @param[in]: vintNum: 虚拟中断号
 * @return:
 *    TTOS_FAIL：<vintType>不是虚拟异常、虚拟tick中断、虚拟外部中断和虚拟服务中断；
 *               <vintType>是虚拟异常或虚拟外部中断或虚拟服务中断，并且<vintNum>不在虚拟中断号范围内。
 *    TTOS_OK：卸载成功。
 * @implements: DT.6.5
 */
T_TTOS_ReturnCode TTOS_UninstallVintHandler (T_VMK_VintType vintType,
                                             T_UWORD        vintNum)
{
    T_WORD ret;

    ret = ttos_pic_irq_uninstall (vintNum, NULL);

    /* @REPLACE_BRACKET: ret */
    return (ret);
}

/* @IGNORE_END: */

/**
 * @brief:
 *    安装指定虚拟外部中断的用户处理程序。
 * @param[in]: intNum: 虚拟外部中断号
 * @param[in]: handler: 虚拟外部中断入口函数
 * @param[out]: oldHandler: 没有使用
 * @return:
 *    TTOS_FAIL：<intNum>不在虚拟外部中断号范围内。
 *    TTOS_OK：安装成功。
 * @implements: DT.6.4
 */
T_TTOS_ReturnCode TTOS_InstallIntHandler (T_UBYTE             intNum,
                                          T_TTOS_ISR_HANDLER  handler,
                                          T_TTOS_ISR_HANDLER *oldHandler)
{
    T_WORD ttosRet;

    ttosRet
        = ttos_pic_irq_install (intNum, (isr_handler_t)handler, NULL, 0, NULL);
    return (ttosRet);
}
/* @IGNORE_BEGIN: */

/**
 * @brief:
 *    使能当前用户分区指定的虚拟外部中断。
 * @param[in]: intNum: 虚拟外部中断号
 * @return:
 *    TTOS_FAIL: 失败，<intNum>不在虚拟外部中断号范围内。
 *    TTOS_MASKED：<intNum>指定的虚拟外部中断被当前用户分区屏蔽。
 *    TTOS_INVALID_STATE：当前分区的虚拟中断尚未初始化；
 *                       根据中断映射表无法将虚拟外部中断号映射到硬件中断号；
 *                       转换后的硬件中断号大于等于硬件中断个数。
 *    TTOS_OK: 成功。
 */
T_TTOS_ReturnCode TTOS_EnablePIC (T_UBYTE intNum)
{
    T_WORD ret;

    ret = ttos_pic_irq_unmask (intNum);
    return (ret);
}

/**
 * @brief:
 *    禁止当前用户分区指定的虚拟外部中断。
 * @param[in]: intNum: 虚拟外部中断号
 * @return:
 *    TTOS_FAIL: 失败，<intNum>不在虚拟外部中断号范围内。
 *    TTOS_MASKED：<intNum>指定的虚拟外部中断被当前用户分区屏蔽。
 *    TTOS_INVALID_STATE：当前分区的虚拟中断尚未初始化；
 *                       根据中断映射表无法将虚拟外部中断号映射到硬件中断号；
 *                       转换后的硬件中断号大于等于硬件中断个数。
 *    TTOS_OK: 成功。
 */
T_TTOS_ReturnCode TTOS_DisablePIC (T_UBYTE intNum)
{
    T_WORD ret;

    ret = ttos_pic_irq_mask (intNum);
    return (ret);
}

/*
 * @brief:
 *    判断是否在中断处理程序中。
 * @return:
 *    TRUE: 在中断处理程序中。
 *    FALSE: 不在中断处理程序中。
 * @tracedREQ: RT
 * @implements: DT.9.13
 */
T_BOOL ttosIsISR (void)
{
    return (0 != ttosGetIntNestLevel ());
}
