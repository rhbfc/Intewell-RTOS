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
#include <commonTypes.h>
#include <commonUtils.h>
#include <stddef.h>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
T_EXTERN T_UWORD zero_num_of_word_get (T_UWORD numVar);
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *     从源区域拷贝指定字节的数据到目的区域。
 * @param[in]: src:  源地址
 * @param[in]: size: 需要拷贝的内存字节数
 * @param[in]: alignType: 对齐方式。单字节对齐为ALIGNED_BY_ONE_BYTE，
 *                         双字节对齐为ALIGNED_BY_TWO_BYTE，四字节对齐为
 *                         ALIGNED_BY_FOUR_BYTE
 * @param[out]: dest:目的地址
 * @return:
 *     无
 */
T_VOID memAlignCpy (T_VOID *dest, T_VOID *src, T_UWORD size,
                    T_AlignType alignType)
{
    T_UBYTE  *destByOne  = NULL;
    T_UBYTE  *srcByOne   = NULL;
    T_UHWORD *destByTwo  = NULL;
    T_UHWORD *srcByTwo   = NULL;
    T_UWORD  *destByFour = NULL;
    T_UWORD  *srcByFour  = NULL;

    /* @REPLACE_BRACKET: 对齐方式 */
    switch (alignType)
    {
    case ALIGNED_BY_ONE_BYTE:

        destByOne = dest;
        srcByOne  = src;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT: 按字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByOne) = (*srcByOne);
            destByOne++;
            srcByOne++;
        }

        break;

    case ALIGNED_BY_TWO_BYTE:

        destByTwo = dest;
        srcByTwo  = src;

        /* @KEEP_COMMENT: 设置<size>为<size>除以2 */
        size = size >> 1;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT:
             * 按双字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 双字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByTwo) = (*srcByTwo);
            destByTwo++;
            srcByTwo++;
        }

        break;

    default:

        destByFour = dest;
        srcByFour  = src;

        /* @KEEP_COMMENT: 设置<size>为<size>除以4 */
        size = size >> 2;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT:
             * 按四字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 四字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByFour) = (*srcByFour);
            destByFour++;
            srcByFour++;
        }

        break;
    }
}

/*
 * @brief:
 *     对指定空间清0。
 * @param[in]: size: 需要清0的内存字节数
 * @param[in]: alignType: 对齐方式。单字节对齐为ALIGNED_BY_ONE_BYTE，
 *                         双字节对齐为ALIGNED_BY_TWO_BYTE，四字节对齐为
 *                         ALIGNED_BY_FOUR_BYTE
 * @param[out]: dest:目的地址
 * @return:
 *     无
 */
T_VOID memAlignClear (T_VOID *dest, T_UWORD size, T_AlignType alignType)
{
    T_UBYTE  *destByOne  = NULL;
    T_UHWORD *destByTwo  = NULL;
    T_UWORD  *destByFour = NULL;

    /* @REPLACE_BRACKET: 对齐方式 */
    switch (alignType)
    {
    case ALIGNED_BY_ONE_BYTE:

        destByOne = dest;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT: 按字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByOne) = 0;
            destByOne++;
        }

        break;

    case ALIGNED_BY_TWO_BYTE:

        destByTwo = dest;

        /* @KEEP_COMMENT: 设置<size>为<size>除以2 */
        size = size >> 1;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT:
             * 按双字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 双字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByTwo) = 0;
            destByTwo++;
        }

        break;

    default:

        destByFour = dest;

        /* @KEEP_COMMENT: 设置<size>为<size>除以4 */
        size = size >> 2;

        /* @REPLACE_BRACKET: <size>--不为0 */
        while ((size--) != 0)
        {
            /*
             * @KEEP_COMMENT:
             * 按四字节长度拷贝<src>当前数据到<dest>当前位置，并按
             * 四字节长度往后移动<src>和<dest>当前拷贝位置
             */
            (*destByFour) = 0;
            destByFour++;
        }

        break;
    }
}

/*
 * @brief
 *     获取变量的最低有效位。
 * @param[in] numVar: 变量。
 * @returns:
 *   最低有效位。
 */
T_UWORD word_lsb_get (T_UWORD numVar)
{
    T_UWORD i = 0;

    T_UWORD temp = numVar;

    for (i = 0; i < UWORD_BIT_COUNT; i++)
    {
        if ((temp & 0x00000001) == 0x00000001)
        {
            break;
        }

        temp = temp >> 0x1;
    }

    return (i);
}
