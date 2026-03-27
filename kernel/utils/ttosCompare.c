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
 * @brief:
 *    比较两个对象名字是否相等，并返回比较结果。
 * @param[in]: name1: 参与比较的第一个名字
 * @param[in]: name2: 参与比较的第二个名字
 * @return:
 *    TRUE: 名字相等。
 *    FALSE: 名字不相等或者对象名字长度大于DeltaTT对象名字的最大长度32字节。
 * @tracedREQ: RT.9
 * @implements: DT.9.1
 */
T_BOOL ttosCompareName (T_UBYTE *name1, T_UBYTE *name2)
{
    T_UWORD i;

    /* 名字相等，则必须字符大小、名字长度都必须相等 */

    /*
     * @REPLACE_BRACKET:
     * 变量i=0;i<=DeltaTT对象名字的最大长度TTOS_OBJECT_NAME_LENGTH; i++
     */
    for (i = U (0); i <= TTOS_OBJECT_NAME_LENGTH; i++)
    {
        /* @REPLACE_BRACKET: <name1>当前字符为'\0' || <name2>当前字符为'\0' */
        if (('\0' == (*name1)) || ('\0' == (*name2)))
        {
            break;
        }

        /* @REPLACE_BRACKET: <name1>当前字符不等于<name2>当前字符 */
        if (*name1 != *name2)
        {
            break;
        }

        /* @KEEP_COMMENT: 依字节向后移动<name1>和<name2>当前位置 */
        name1++;
        name2++;
    }

    /* @REPLACE_BRACKET: <name1>当前字符为'\0' && <name2>当前字符为'\0' */
    if (('\0' == (*name1)) && ('\0' == (*name2)))
    {
        /* @REPLACE_BRACKET: TRUE */
        return (TRUE);
    }

    /* @REPLACE_BRACKET: FALSE */
    return (FALSE);
}

/*
 * @brief:
 *    比较两个对象版本是否相等，并返回比较结果。
 * @param[in]:version1: 参与比较的第一个版本
 * @param[in]: version2: 参与比较的第二个版本
 * @return:
 *    TRUE: 版本相等。
 *    FALSE: 版本不相等或者对象名字长度大于DeltaTT对象版本的最大长度32字节。
 */
T_UWORD ttosCompareVerison (T_UBYTE *version1, T_UBYTE *version2)
{
    T_UWORD i;

    /* 版本相等，则必须字符大小、版本长度都必须相等 */

    /*
     * @REPLACE_BRACKET:
     * 变量i=0;i<=DeltaTT对象版本的最大长度TTOS_OBJECT_VERSION_LENGTH; i++
     */
    for (i = U (0); i <= TTOS_OBJECT_VERSION_LENGTH; i++)
    {
        /* @REPLACE_BRACKET: <name1>当前字符为'\0' || <version2>当前字符为'\0'
         */
        if (('\0' == (*version1)) || ('\0' == (*version2)))
        {
            break;
        }

        /* @REPLACE_BRACKET: <version1>当前字符不等于<version2>当前字符 */
        if (*version1 != *version2)
        {
            break;
        }

        /* @KEEP_COMMENT: 依字节向后移动<version1>和<version2>当前位置 */
        version1++;
        version2++;
    }

    /* @REPLACE_BRACKET: <version1>当前字符为'\0' && <version2>当前字符为'\0' */
    if (('\0' == (*version1)) && ('\0' == (*version2)))
    {
        /* @REPLACE_BRACKET: TRUE */
        return (TRUE);
    }

    /* @REPLACE_BRACKET: FALSE */
    return (FALSE);
}
