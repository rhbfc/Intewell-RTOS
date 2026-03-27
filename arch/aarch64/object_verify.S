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
 * @file： object_verify.S
 * @brief：
 *      <li>提供对象有效性检查相关的汇编接口。</li>
 * @implements：
 */

/************************头 文 件******************************/
#define ASM_USE
#include <asm.h>
#include <cpu.h>
#include <ttosHandle.h>

/************************宏 定 义******************************/
/************************实    现******************************/
.global ttosVerifyObjectCore
.global ttosVerifyObjectCoreMagicRead;
.global ttosVerifyObjectCoreTypeRead;
.global ttosVerifyObjectCoreError;

/*
* @brief
*       判断对象句柄是否有效。
* @param[in]  objCore: 指向对象核心结构的指针
* @param[in]  type: 期望比较的对象类型
* @return
*       TTOS_VERIFY_OBJCORE_OK: 对象有效。
*       TTOS_VERIFY_OBJCORE_FAIL: 对象无效。
* @implements  
*/

/*
T_UWORD ttosVerifyObjectCore(
T_TTOS_ObjectCoreID  objectCoreID,	
T_UWORD          type		
)
*/
ENTRY(ttosVerifyObjectCore)

    /* x0 = objectCoreID, x1 = type */
    cmp x0, #0
    beq ttosVerifyObjectCoreError

ttosVerifyObjectCoreMagicRead:

    ldr x10, [x0, #HANDLE_MAGIC]       /* x10 = objectCoreID->handleId.magic */
    cmp	x0, x10                         /* objectCoreID == objectCoreID->handleId.magic? */
    b.ne	ttosVerifyObjectCoreError      /* no -> return TTOS_VERIFY_OBJCORE_FAIL */
     
ttosVerifyObjectCoreTypeRead:

    ldr x10, [x0, #HANDLE_TYPE]        /* x10 = objectCoreID->handleId.type */
    cmp	x1, x10                        /* type == objectCoreID->handleId.type? */
    b.ne	ttosVerifyObjectCoreError      /* no -> return TTOS_VERIFY_OBJCORE_FAIL */

    ldr w0, =TTOS_VERIFY_OBJCORE_OK
    ret                                               /* return (TTOS_VERIFY_OBJCORE_OK) */

    /*
     * 如果异常来自 ttosVerifyObjectCore()，异常处理函数
     * ttosVerifyObjectCoreTrap() 会把异常上下文中的 PC 改写为本标签。
     */

ttosVerifyObjectCoreError:

    ldr w0, =TTOS_VERIFY_OBJCORE_FAIL
    ret                                                 /* return (TTOS_VERIFY_OBJCORE_FAIL) */

ENDPROC(ttosVerifyObjectCore)
