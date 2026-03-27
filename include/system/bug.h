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

#ifndef _ASM_GENERIC_BUG_H
#define _ASM_GENERIC_BUG_H

#include <commonUtils.h>

#ifndef	WARN
#define WARN(no, str...)                printk(str)
#define WARN_ON(condition) ({                   \
    int __ret_warn_on = !!(condition);          \
    unlikely(__ret_warn_on);                    \
})
#endif

#ifndef HAVE_ARCH_BUG
#define BUG() do {		\
	printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__);		\
	do {} while (1);	\
} while (0)
#endif


#ifndef HAVE_ARCH_BUG_ON
#ifndef BUG_ON
#define BUG_ON(condition) do { if (unlikely(condition)) BUG(); } while (0)
#endif
#endif

#endif