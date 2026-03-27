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
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实    现******************************/

/* @MODULE> */

/*
 * @brief
 *       判断最大长度范围内的对象名是否有效。
 * @param[in]  name: 对象名
 * @param[in]  length: 最大名字长度，不包含结束符。
 * @return
 *       FALSE:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符，name为空串或者空指针
 *       TRUE: 对象名name字符串符合命名规范
 * @implements
 */
T_BOOL ttosVerifyName (T_UBYTE *name, T_UWORD length)
{
    T_UWORD count = 0U;

    /* @REPLACE_BRACKET: 对象名为空或者空串，返回错误 */
    if ((name == NULL) || (*name == (T_UBYTE)'\0'))
    {
        /* @REPLACE_BRACKET:  FALSE*/
        return (FALSE);
    }

    /* @KEEP_COMMENT:  对象名字符串ascii码取值范围在[32,126]之间 */
    /* @REPLACE_BRACKET:  (*name != '\0') && (count < length)*/
    while ((*name != (T_UBYTE)'\0') && (count < length))
    {
        /* @REPLACE_BRACKET:  (*name <= 31) || (*name >= 127)*/
        if ((*name <= 31U) || (*name >= 127U))
        {
            /* @REPLACE_BRACKET:  FALSE*/
            return (FALSE);
        }

        name++;
        count++;
    }

    /* @REPLACE_BRACKET:  TRUE*/
    return (TRUE);
}

/*
 * @brief
 *       改变默认的对象名为当前任务的名字。
 * @param[in]  name1: 源对象名
 * @param[in]  name2: 默认象名
 * @return
 *       无
 * @implements: RTE_DUTILS.10.2
 */
void ttosChangeName (T_UBYTE *name1, T_UBYTE *Name2)
{
    /* @KEEP_COMMENT:  比较两字符串 */
    /* @REPLACE_BRACKET:  TRUE == ttosCompareName(name1, Name2)*/
    if (TRUE == ttosCompareName (name1, Name2))
    {
        ttosCopyName (ttosGetRunningTaskName (), name1);
    }
}
