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

#ifndef __VDSO_DATAPAGE_H__
#define __VDSO_DATAPAGE_H__

struct ttos_vdso_data
{
    unsigned long long cycle_freq;
    long long realtime_offset_ns;
    unsigned long long clock_res_ns;
    unsigned long long reserved[3];
};

#endif /* __VDSO_DATAPAGE_H__ */