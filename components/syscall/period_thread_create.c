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

#include <errno.h>
#include <period_sched_group.h>
#include <ttosProcess.h>
#include <uaccess.h>


int period_thread_create (arch_exception_context_t *context)
{
    struct period_param         kparam;
    struct period_param __user *uparam;
    unsigned long               clone_flags;
    unsigned long               newsp;
    int * __user               set_child_tid;
    int * __user               clear_child_tid;
    unsigned long               tls;
#ifdef __aarch64__
    uparam = (struct period_param __user *)arch_context_get_args (context, 5);
#elif defined(__arm__)
    uparam = (struct period_param __user *)arch_context_get_args (context, 8);
#endif
    clone_flags = (unsigned long)arch_context_get_args (context, 0);
    newsp       = (unsigned long)arch_context_get_args (context, 1);
    set_child_tid = (int __user *)arch_context_get_args (context, 2);
    clear_child_tid = (int __user *)arch_context_get_args (context, 4);
    tls         = (unsigned long)arch_context_get_args (context, 3);
    

    if (copy_from_user (&kparam, uparam, sizeof (kparam)))
    {
        return -EINVAL;
    }

    return do_fork (clone_flags, newsp, set_child_tid, clear_child_tid, tls, &kparam);
    
}
