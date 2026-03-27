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

#ifndef __PERCPU_H__
#define __PERCPU_H__

#include "stddef.h"
#include "ttos.h"
#include <cpuid.h>

#define __percpu    

#ifdef CONFIG_SMP
#define PER_CPU_BASE_SECTION ".data..percpu"
#else
#define PER_CPU_BASE_SECTION ".data"
#endif

extern  char __per_cpu_load[];
extern  char __per_cpu_start[];
extern  char __per_cpu_end[];

#ifndef __per_cpu_offset
extern unsigned long __per_cpu_offset[CONFIG_MAX_CPUS];

#define per_cpu_offset(x) (__per_cpu_offset[x])
#endif


#define __PCPU_ATTRS(sec)                                       \
    __percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))

/*
 * Normal declaration and definition macros.
 */
#define DECLARE_PER_CPU_SECTION(type, name, sec)                \
    extern __PCPU_ATTRS(sec) __typeof__(type) name

#define DEFINE_PER_CPU_SECTION(type, name, sec)                 \
    __PCPU_ATTRS(sec) __typeof__(type) name


/*
 * Variant on the per-CPU variable declaration/definition theme used for
 * ordinary per-CPU variables.
 */
#define DECLARE_PER_CPU(type, name)                             \
    DECLARE_PER_CPU_SECTION(type, name, "")

#define DEFINE_PER_CPU(type, name)                              \
    DEFINE_PER_CPU_SECTION(type, name, "")

#define PER_CPU_SIZE        (size_t) (__per_cpu_end - __per_cpu_start)
#define PER_CPU_START_VADDR (void*)__per_cpu_start

#ifndef RELOC_HIDE
# define RELOC_HIDE(ptr, off)                                       \
    ({ unsigned long __ptr;                                         \
        __ptr = (unsigned long) (ptr);                              \
        (typeof(ptr)) (__ptr + (off)); })
#endif

/*
 * __verify_pcpu_ptr() verifies @ptr is a percpu pointer without evaluating
 * @ptr and is invoked once before a percpu area is accessed by all
 * accessors and operations.  This is performed in the generic part of
 * percpu and arch overrides don't need to worry about it; however, if an
 * arch wants to implement an arch-specific percpu accessor or operation,
 * it may use __verify_pcpu_ptr() to verify the parameters.
 *
 * + 0 is required in order to convert the pointer type from a
 * potential array type to a pointer to a single item of the array.
 */
#define __verify_pcpu_ptr(ptr)                                      \
do {                                                                \
    const void __percpu *__vpp_verify = (typeof((ptr) + 0))NULL;    \
    (void)__vpp_verify;                                             \
} while (0)

#ifdef CONFIG_SMP

/*
 * Add an offset to a pointer but keep the pointer as-is.  Use RELOC_HIDE()
 * to prevent the compiler from making incorrect assumptions about the
 * pointer value.  The weird cast keeps both GCC and sparse happy.
 */
#define SHIFT_PERCPU_PTR(__p, __offset)                             \
    RELOC_HIDE((typeof(*(__p)) *) (__p), (__offset))

#define per_cpu_ptr(ptr, cpu)                                       \
({                                                                  \
    __verify_pcpu_ptr(ptr);                                         \
    SHIFT_PERCPU_PTR((ptr), per_cpu_offset((cpu)));                 \
})

#define raw_cpu_ptr(ptr)                                            \
({                                                                  \
    __verify_pcpu_ptr(ptr);                                         \
    SHIFT_PERCPU_PTR((ptr), per_cpu_offset(cpuid_get()));           \
})

#ifdef CONFIG_DEBUG_PREEMPT
#define this_cpu_ptr(ptr)                                           \
({                                                                  \
    __verify_pcpu_ptr(ptr);                                         \
    SHIFT_PERCPU_PTR(ptr, my_cpu_offset);                           \
})
#else
#define this_cpu_ptr(ptr) raw_cpu_ptr(ptr)
#endif

#else    /* CONFIG_SMP */

#define VERIFY_PERCPU_PTR(__p)                                      \
({                                                                  \
    __verify_pcpu_ptr(__p);                                         \
    (typeof(*(__p)) *)(__p);                                        \
})

#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu); VERIFY_PERCPU_PTR(ptr); })
#define raw_cpu_ptr(ptr)        per_cpu_ptr(ptr, 0)
#define this_cpu_ptr(ptr)       raw_cpu_ptr(ptr)

#endif    /* CONFIG_SMP */

#define per_cpu(var, cpu)       (*per_cpu_ptr(&(var), cpu))

#define preempt_disable()        TTOS_DisablePreempt()
#define preempt_enable()         TTOS_EnablePreempt()

/*
 * Must be an lvalue. Since @var must be a simple identifier,
 * we force a syntax error here if it isn't.
 */
#define get_cpu_var(var)                                            \
(*({                                                                \
    preempt_disable();                                              \
    this_cpu_ptr(&var);                                             \
}))

/*
 * The weird & is necessary because sparse considers (void)(var) to be
 * a direct dereference of percpu variable (var).
 */
#define put_cpu_var(var)                                            \
do {                                                                \
    (void)&(var);                                                   \
    preempt_enable();                                               \
} while (0)

#define get_cpu_ptr(var)                                            \
({                                                                  \
    preempt_disable();                                              \
    this_cpu_ptr(var);                                              \
})

#define put_cpu_ptr(var)                                            \
do {                                                                \
    (void)(var);                                                    \
    preempt_enable();                                               \
} while (0)


#endif  /* __PERCPU_H__ */