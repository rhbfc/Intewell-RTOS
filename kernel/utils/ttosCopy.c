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
 *    从源区域拷贝对象名字(对象名字以字符串结束符'\0'结束)到目的区域。如果源区
 *    域的对象名字长度大于TTOS对象名字的最大长度32字节时，只拷贝32字节。
 * @param[in]: src:  源起始地址
 * @param[out]: dest:目的起始地址
 * @return:
 *    无
 * @tracedREQ: RT.9
 * @implements: DT.9.2
 */
void ttosCopyName (T_UBYTE *src, T_UBYTE *dest)
{
    T_UWORD cnt;
    /* @KEEP_COMMENT: 定义拷贝名字长度cnt初始为0 */
    cnt = U (0);

    /*
     * @REPLACE_BRACKET: cnt小于DeltaTT对象名字的最大长度TTOS_OBJECT_NAME_LENGTH
     * && src当前字符不为'\0'
     */
    while ((cnt < TTOS_OBJECT_NAME_LENGTH) && ('\0' != (*src)))
    {
        /*
         * @KEEP_COMMENT: 按字节拷贝<src>当前数据到<dest>当前位置，
         * 并按字节往后移动<src>和<dest>当前拷贝位置
         */
        *dest = *src;
        dest++;
        src++;
        /* @KEEP_COMMENT: cnt加1 */
        cnt++;
    }

    /* @KEEP_COMMENT: 设置<dest>当前位置为'\0' */
    *dest = (T_UBYTE)'\0';
}

/*
 * @brief:
 *    从源区域拷贝对象版本(对象版本以字符串结束符'\0'结束)到目的区域。如果源区
 *    域的对象版本长度大于TTOS对象版本的最大长度32字节时，只拷贝32字节。
 * @param[in]: src:  源起始地址
 * @param[out]: dest:目的起始地址
 * @return:
 *    无
 */
void ttosCopyVersion (T_UBYTE *src, T_UBYTE *dest)
{
    T_UWORD cnt;
    /* @KEEP_COMMENT: 定义拷贝版本长度cnt初始为0 */
    cnt = U (0);

    /*
     * @REPLACE_BRACKET:
     * cnt小于DeltaTT对象版本的最大长度TTOS_OBJECT_VERSION_LENGTH &&
     * src当前字符不为'\0'
     */
    while ((cnt < TTOS_OBJECT_VERSION_LENGTH) && ('\0' != (*src)))
    {
        /*
         * @KEEP_COMMENT: 按字节拷贝<src>当前数据到<dest>当前位置，
         * 并按字节往后移动<src>和<dest>当前拷贝位置
         */
        *dest = *src;
        dest++;
        src++;
        /* @KEEP_COMMENT: cnt加1 */
        cnt++;
    }

    /* @KEEP_COMMENT: 设置<dest>当前位置为'\0' */
    *dest = (T_UBYTE)'\0';
}
