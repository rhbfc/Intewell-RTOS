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

/**
 * @file： atomic.S
 * @brief：
 *     <li>AArch64 原子操作实现。</li>
 */

/************************头 文 件******************************/
#define ASM_USE
#include <asm.h>
#include <arch_helpers.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************函数实现******************************/

.text

/*
* @brief 
*    将target指向的值增加value，并返回target原有指向的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target原有指向的值
*/
/* T_UWORD atomic32_add(T_UWORD * target, T_UWORD value) */
ENTRY(atomic32_add)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	add	w3, w2, w1	/* add value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_add)	

/*
* @brief 
*	 将target指向的值增加value，并返回target增加后的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target增加后的值
*/
/* T_UWORD atomic32_add_return(T_UWORD * target, T_UWORD value) */
ENTRY(atomic32_add_return)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	add	w3, w2, w1	/* add value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w3		/* return new value */
	ret
ENDPROC(atomic32_add_return)

/*
* @brief 
*	 将target指向的值增加value，并返回target增加后的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target增加前的值
*/
/* T_UWORD atomic32_fetch_add(T_UWORD * target, T_UWORD value) */
ENTRY(atomic32_fetch_add)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	add	w3, w2, w1	/* add value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_fetch_add)

/*
* @brief 
*       将target指向的值与value位或，并返回target原有的值。
* @param  target: 需要位或值的地址。
* @param[in]  value: 需要位或的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic32_or(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_or)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	orr	w3, w2, w1	/* OR value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_or)
     
/*
* @brief 
*       将target指向的值与value位与，并返回target原有的值。
* @param  target: 需要位与值的地址。
* @param[in]  value: 需要位与的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic32_and(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_and)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	and	w3, w2, w1	/* AND value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_and)

/*
* @brief 
*       设置target指向的值为value，并返回target原有的值。
* @param  target: 需要设置值的地址。
* @param[in]  value: 需要设置的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic32_set(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_set)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	stlxr	w4, w1, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_set)

/*
* @brief
*       获取target指向的值。
* @param  target: 需要获取值的地址。
* @return target指向的值
*/
/* T_UWORD atomic32_get(T_UWORD * target) */
ENTRY(atomic32_get)
	ldr	   w0, [x0]        
	ret
ENDPROC(atomic32_get)

/*
* @brief
*       将target指向的值增加1，并返回target原有的值。
* @param  target: 需要增加值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic32_inc(T_UWORD * target) */
ENTRY(atomic32_inc)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	add	w3, w2, #1	/* add 1 */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_inc)

/*
* @brief
*		将target指向的值增加1，并返回target增加后的值。
* @param  target: 需要增加值的地址。
* @return target增加后的值
*/
/* T_UWORD atomic32_inc_return(T_UWORD * target) */
ENTRY(atomic32_inc_return)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	add	w3, w2, #1	/* add 1 */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w3		/* return new value */
	ret
ENDPROC(atomic32_inc_return)

/*
* @brief
*       将target指向的值减去1，并返回target原有的值。
* @param  target: 需要减去值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic32_dec(T_UWORD * target) */
ENTRY(atomic32_dec)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	sub	w3, w2, #1	/* subtract 1 */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_dec)

/*
* @brief
*		将target指向的值减去1，并返回target减去后的值。
* @param  target: 需要减去值的地址。
* @return target减去后的值
*/
/* T_UWORD atomic32_dec_return(T_UWORD * target) */
ENTRY(atomic32_dec_return)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	sub	w3, w2, #1	/* subtract 1 */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w3		/* return new value */
	ret
ENDPROC(atomic32_dec_return)

/*
* @brief 
*       将target指向的值与oldValue对比，如果相等，设置target指向的值为newValue，
*       并返回TRUE；如果不相等，则直接返回FALSE。
* @param  target: 需要对比值的地址。
* @param[in]  oldValue: 需要对比的值。
* @param[in]  newValue: 需要设置的值。
* @return TRUE: target指向的值与oldValue相等
* @return FALSE:target指向的值与oldValue不相等
*/
/* BOOL atomic32_cas(T_UWORD * target,T_UWORD oldValue,T_UWORD newValue) */
ENTRY(atomic32_cas)
	dmb     ish              /* full barrier before CAS */

cas32_loop:
    ldaxr   w3, [x0]         /* load-acquire exclusive (32-bit) */
    cmp     w3, w1           /* compare with expected */
    b.ne    cas32_fail       /* fail if not equal */

    stlxr   w4, w2, [x0]     /* store-release exclusive (32-bit) */
    cbnz    w4, cas32_loop   /* retry if store failed */

    dmb     ish              /* full barrier after successful CAS */
    mov     w0, #1           /* return true */
    ret

cas32_fail:
    dmb     ish              /* full barrier before return */
    mov     w0, #0           /* return false */
    ret
ENDPROC(atomic32_cas)

/*
* @brief 
*       将target指向的值减去value，并返回target原有的值。
* @param  target: 需要减去值的地址。
* @param[in]  value: 需要减去的值。
* @return target原有指向的值
*/
/* T_UWORD atomic32_sub(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_sub)
1:
	ldaxr	w2, [x0]	/* load target value and mark exclusive access */
	sub	w3, w2, w1	/* sub value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_sub)

/*
* @brief 
*		将target指向的值减去value，并返回target减去后的值。
* @param  target: 需要减去值的地址。
* @param[in]  value: 需要减去的值。
* @return target减去后的值
*/
/* T_UWORD atomic32_sub_return(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_sub_return)
1:
	ldaxr	w2, [x0]	/* load target value and mark exclusive access */
	sub	w3, w2, w1	/* sub value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w3		/* return new value */
	ret
ENDPROC(atomic32_sub_return)

/*
* @brief 
*       清除target指向的值，并返回target原有的值。
* @param  target: 需要清除值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic32_clear(T_UWORD * target) */
ENTRY(atomic32_clear)
	mov x1, #0
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	stlxr	w4, w1, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_clear)

/*
* @brief 
*       将target指向的值与value位异或，并返回target原有的值。
* @param  target: 需要位异或值的地址。
* @param[in]  value: 需要位异或的值。
* @return target原有指向的值
*/
/* T_UWORD atomic32_xor(T_UWORD * target,T_UWORD value) */
ENTRY(atomic32_xor)
1:
	ldaxr	w2, [x0]	/* load old value and mark exclusive access */
	eor	w3, w2, w1	/* XOR value */
	stlxr	w4, w3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	w0, w2		/* return old value */
	ret
ENDPROC(atomic32_xor)

/*
* @brief 
*    将target指向的值增加value，并返回target原有指向的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target原有指向的值
*/
/* T_UWORD atomic64_add(T_UWORD * target, T_UWORD value) */
ENTRY(atomic64_add)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	add	x3, x2, x1	/* add value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_add)	

/*
* @brief 
*	 将target指向的值增加value，并返回target增加后的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target增加后的值
*/
/* T_UDWORD atomic64_add_return(T_UDWORD * target, T_UDWORD value) */
ENTRY(atomic64_add_return)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	add	x3, x2, x1	/* add value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x3		/* return new value */
	ret
ENDPROC(atomic64_add_return)

/*
* @brief 
*	 将target指向的值增加value，并返回target增加后的值。
* @param[out]||[in]: target: 需要增加值的地址。
* @param[in]: value: 需要增加的值
* @retval target增加前的值
*/
/* T_UDWORD atomic64_fetch_add(T_UDWORD * target, T_UDWORD value) */
ENTRY(atomic64_fetch_add)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	add	x3, x2, x1	/* add value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_fetch_add)


/*
* @brief 
*       将target指向的值与value位或，并返回target原有的值。
* @param  target: 需要位或值的地址。
* @param[in]  value: 需要位或的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic64_or(T_UWORD * target,T_UWORD value) */
ENTRY(atomic64_or)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	orr	x3, x2, x1	/* OR value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_or)
     
/*
* @brief 
*       将target指向的值与value位与，并返回target原有的值。
* @param  target: 需要位与值的地址。
* @param[in]  value: 需要位与的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic64_and(T_UWORD * target,T_UWORD value) */
ENTRY(atomic64_and)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	and	x3, x2, x1	/* AND value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_and)

/*
* @brief 
*       设置target指向的值为value，并返回target原有的值。
* @param  target: 需要设置值的地址。
* @param[in]  value: 需要设置的值。
* @return target原有指向的值。
*/
/* T_UWORD atomic64_set(T_UWORD * target,T_UWORD value) */
ENTRY(atomic64_set)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	stlxr	w4, x1, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_set)

/*
* @brief 
*       获取target指向的值。
* @param  target: 需要获取值的地址。
* @return target指向的值。
*/
/* T_UWORD atomic64_get(T_UWORD * target) */
ENTRY(atomic64_get)
	ldr	x0, [x0]        
	ret
ENDPROC(atomic64_get)

/*
* @brief
*       将target指向的值增加1，并返回target原有的值。
* @param  target: 需要增加值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic64_inc(T_UWORD * target) */
ENTRY(atomic64_inc)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	add	x3, x2, #1	/* add 1 */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_inc)

/*
* @brief
*		将target指向的值增加1，并返回target增加后的值。
* @param  target: 需要增加值的地址。
* @return target增加后的值
*/
/* T_UWORD atomic64_inc_return(T_UWORD * target) */
ENTRY(atomic64_inc_return)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	add	x3, x2, #1	/* add 1 */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x3		/* return new value */
	ret
ENDPROC(atomic64_inc_return)

/*
* @brief
*       将target指向的值减去1，并返回target原有的值。
* @param  target: 需要减去值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic64_dec(T_UWORD * target) */
ENTRY(atomic64_dec)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	sub	x3, x2, #1	/* subtract 1 */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_dec)

/*
* @brief
*		将target指向的值减去1，并返回target减去后的值。
* @param  target: 需要减去值的地址。
* @return target减去后的值
*/
/* T_UWORD atomic64_dec_return(T_UWORD * target) */
ENTRY(atomic64_dec_return)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	sub	x3, x2, #1	/* subtract 1 */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x3		/* return new value */
	ret
ENDPROC(atomic64_dec_return)

/*
* @brief 
*       将target指向的值与oldValue对比，如果相等，设置target指向的值为newValue，
*       并返回TRUE；如果不相等，则直接返回FALSE。
* @param  target: 需要对比值的地址。
* @param[in]  oldValue: 需要对比的值。
* @param[in]  newValue: 需要设置的值。
* @return TRUE: target指向的值与oldValue相等
* @return FALSE:target指向的值与oldValue不相等
*/
/* BOOL atomic64_cas(T_UDWORD * target,T_UDWORD oldValue,T_UDWORD newValue) */
ENTRY(atomic64_cas)
	dmb    ish               /* full barrier before CAS */

cas_loop:
    ldaxr  x3, [x0]          /* load-acquire exclusive (64-bit) */
    cmp    x3, x1            /* compare with expected */
    b.ne   cas_fail          /* fail if not equal */
    stlxr  w4, x2, [x0]      /* store-release exclusive (64-bit) */
    cbnz   w4, cas_loop      /* retry if store failed */

    dmb    ish               /* full barrier after successful CAS */
    mov    w0, #1            /* return true */
    ret

cas_fail:
    dmb    ish               /* full barrier before return */
    mov    w0, #0            /* return false */
    ret
ENDPROC(atomic64_cas)

/*
* @brief 
*       将target指向的值减去value，并返回target原有的值。
* @param  target: 需要减去值的地址。
* @param[in]  value: 需要减去的值。
* @return target原有指向的值
*/
/* T_UDWORD atomic64_sub(T_UDWORD * target,T_UDWORD value) */
ENTRY(atomic64_sub)
1:
	ldaxr	x2, [x0]	/* load target value and mark exclusive access */
	sub	x3, x2, x1	/* sub value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_sub)

/*
* @brief 
*		将target指向的值减去value，并返回target减去后的值。
* @param  target: 需要减去值的地址。
* @param[in]  value: 需要减去的值。
* @return target减去后的值
*/
/* T_UDWORD atomic64_sub_return(T_UDWORD * target,T_UDWORD value) */
ENTRY(atomic64_sub_return)
1:
	ldaxr	x2, [x0]	/* load target value and mark exclusive access */
	sub	x3, x2, x1	/* sub value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x3		/* return new value */
	ret
ENDPROC(atomic64_sub_return)

/*
* @brief 
*       清除target指向的值，并返回target原有的值。
* @param  target: 需要清除值的地址。
* @return target原有指向的值
*/
/* T_UWORD atomic64_clear(T_UWORD * target) */
ENTRY(atomic64_clear)
	mov	x1, #0
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	stlxr	w4, x1, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_clear)

/*
* @brief 
*       将target指向的值与value位异或，并返回target原有的值。
* @param  target: 需要位异或值的地址。
* @param[in]  value: 需要位异或的值。
* @return target原有指向的值
*/
/* T_UWORD atomic64_xor(T_UWORD * target,T_UWORD value) */
ENTRY(atomic64_xor)
1:
	ldaxr	x2, [x0]	/* load old value and mark exclusive access */
	eor	x3, x2, x1	/* XOR value */
	stlxr	w4, x3, [x0]	/* try to store new value */
	tbnz	w4, #0, 1b		/* store successful? if not, retry */

	mov	x0, x2		/* return old value */
	ret
ENDPROC(atomic64_xor)
