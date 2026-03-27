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

#ifndef _NOTIFIER_H
#define _NOTIFIER_H
#include <errno.h>
#include <linux/rwsem.h>
#include <spinlock.h>
#include <ttos.h>

/*
 * Notifier chains are of four types:
 *
 *	Atomic notifier chains: Chain callbacks run in interrupt/atomic
 *		context. Callouts are not allowed to block.
 *	Blocking notifier chains: Chain callbacks run in process context.
 *		Callouts are allowed to block.
 *	Raw notifier chains: There are no restrictions on callbacks,
 *		registration, or unregistration.  All locking and protection
 *		must be provided by the caller.
 *	SRCU notifier chains: A variant of blocking notifier chains, with
 *		the same restrictions.
 *
 * atomic_notifier_chain_register() may be called from an atomic context,
 * but blocking_notifier_chain_register() and srcu_notifier_chain_register()
 * must be called from a process context.  Ditto for the corresponding
 * _unregister() routines.
 *
 * atomic_notifier_chain_unregister(), blocking_notifier_chain_unregister(),
 * and srcu_notifier_chain_unregister() _must not_ be called from within
 * the call chain.
 *
 * SRCU notifier chains are an alternative form of blocking notifier chains.
 * They use SRCU (Sleepable Read-Copy Update) instead of rw-semaphores for
 * protection of the chain links.  This means there is _very_ low overhead
 * in srcu_notifier_call_chain(): no cache bounces and no memory barriers.
 * As compensation, srcu_notifier_chain_unregister() is rather expensive.
 * SRCU notifier chains should be used when the chain will be called very
 * often but notifier_blocks will seldom be removed.
 */

#define __rcu
struct notifier_block;

typedef int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action, void *data);

struct notifier_block
{
    notifier_fn_t notifier_call;
    struct notifier_block __rcu *next;
    int priority;
};

struct blocking_notifier_head
{
    struct rw_semaphore rwsem;
    struct notifier_block __rcu *head;
};

struct atomic_notifier_head
{
    ttos_spinlock_t lock;
    struct notifier_block __rcu *head;
};

struct raw_notifier_head
{
    struct notifier_block __rcu *head;
};

#define ATOMIC_INIT_NOTIFIER_HEAD(name)                                                            \
    do                                                                                             \
    {                                                                                              \
        spin_lock_init(&(name)->lock);                                                             \
        (name)->head = NULL;                                                                       \
    } while (0)
#define RAW_INIT_NOTIFIER_HEAD(name)                                                               \
    do                                                                                             \
    {                                                                                              \
        (name)->head = NULL;                                                                       \
    } while (0)

#define ATOMIC_NOTIFIER_INIT(name)                                                                 \
    {                                                                                              \
        __SPINLOCK_INITIALIZER(lock), .head = NULL                                                 \
    }
#define RAW_NOTIFIER_INIT(name)                                                                    \
    {                                                                                              \
        .head = NULL                                                                               \
    }

#define ATOMIC_NOTIFIER_HEAD(name) struct atomic_notifier_head name = ATOMIC_NOTIFIER_INIT(name)
#define RAW_NOTIFIER_HEAD(name) struct raw_notifier_head name = RAW_NOTIFIER_INIT(name)

extern int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                                          struct notifier_block *nb);
extern int raw_notifier_chain_register(struct raw_notifier_head *nh, struct notifier_block *nb);

extern int atomic_notifier_chain_register_unique_prio(struct atomic_notifier_head *nh,
                                                      struct notifier_block *nb);

extern int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
                                            struct notifier_block *nb);
extern int raw_notifier_chain_unregister(struct raw_notifier_head *nh, struct notifier_block *nb);

extern int atomic_notifier_call_chain(struct atomic_notifier_head *nh, unsigned long val, void *v);
extern int raw_notifier_call_chain(struct raw_notifier_head *nh, unsigned long val, void *v);

extern int raw_notifier_call_chain_robust(struct raw_notifier_head *nh, unsigned long val_up,
                                          unsigned long val_down, void *v);

extern bool atomic_notifier_call_chain_is_empty(struct atomic_notifier_head *nh);

#define NOTIFY_DONE 0x0000      /* Don't care */
#define NOTIFY_OK 0x0001        /* Suits me */
#define NOTIFY_STOP_MASK 0x8000 /* Don't call further */
#define NOTIFY_BAD (NOTIFY_STOP_MASK | 0x0002)
/* Bad/Veto action */
/*
 * Clean way to return from the notifier and stop further calls.
 */
#define NOTIFY_STOP (NOTIFY_OK | NOTIFY_STOP_MASK)

/* Encapsulate (negative) errno value (in particular, NOTIFY_BAD <=> EPERM). */
static inline int notifier_from_errno(int err)
{
    if (err)
        return NOTIFY_STOP_MASK | (NOTIFY_OK - err);

    return NOTIFY_OK;
}

/* Restore (negative) errno value from notify return value. */
static inline int notifier_to_errno(int ret)
{
    ret &= ~NOTIFY_STOP_MASK;
    return ret > NOTIFY_OK ? NOTIFY_OK - ret : 0;
}

/*
 *	Declared notifiers so far. I can imagine quite a few more chains
 *	over time (eg laptop power reset chains, reboot chain (to clean
 *	device units up), device [un]mount chain, module load/unload chain,
 *	low memory chain, screenblank chain (for plug in modular screenblankers)
 *	VC switch chains (for loadable kernel svgalib VC switch helpers) etc...
 */

/* CPU notfiers are defined in include/linux/cpu.h. */

/* netdevice notifiers are defined in include/linux/netdevice.h */

/* reboot notifiers are defined in include/linux/reboot.h. */

/* Hibernation and suspend events are defined in include/linux/suspend.h. */

/* Virtual Terminal events are defined in include/linux/vt.h. */

#define NETLINK_URELEASE 0x0001 /* Unicast netlink socket released */

/* Console keyboard events.
 * Note: KBD_KEYCODE is always sent before KBD_UNBOUND_KEYCODE, KBD_UNICODE and
 * KBD_KEYSYM. */
#define KBD_KEYCODE 0x0001         /* Keyboard keycode, called before any other */
#define KBD_UNBOUND_KEYCODE 0x0002 /* Keyboard keycode which is not bound to any other */
#define KBD_UNICODE 0x0003         /* Keyboard unicode */
#define KBD_KEYSYM 0x0004          /* Keyboard keysym */
#define KBD_POST_KEYSYM 0x0005     /* Called after keyboard keysym interpretation */

#endif /* _LINUX_NOTIFIER_H */
