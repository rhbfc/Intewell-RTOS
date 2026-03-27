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

#ifndef __GFP_TYPES_H__
#define __GFP_TYPES_H__

#include <system/const.h>

/*
 * 当前仓库仅保留实际使用到的分配标志：
 * - GFP_KERNEL: 默认内核分配
 * - GFP_DMA32:  选择 DMA/非缓存堆
 * - __GFP_ZERO: 请求返回零填充分配结果
 */
typedef unsigned int gfp_t;

enum
{
    __GFP_FLAG_ZERO_BIT = 0,
    __GFP_FLAG_DMA32_BIT = 1,
};

#define __GFP_ZERO ((gfp_t)BIT(__GFP_FLAG_ZERO_BIT))
#define __GFP_DMA32 ((gfp_t)BIT(__GFP_FLAG_DMA32_BIT))

#define GFP_KERNEL ((gfp_t)0U)
#define GFP_DMA32 (__GFP_DMA32)

#endif /* __GFP_TYPES_H__ */
