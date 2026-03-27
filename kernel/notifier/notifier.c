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

#include <notifier.h>
#include <uaccess.h>
#include <system/bug.h>

#define rcu_assign_pointer(p, v)                                                                   \
    do                                                                                             \
    {                                                                                              \
        (p) = (v);                                                                                 \
    } while (0)
#define rcu_dereference_raw(p) p
#define trace_notifier_register(func)
#define trace_notifier_unregister(func)
#define trace_notifier_run(func)
#define rcu_read_lock()
#define rcu_read_unlock()
#define synchronize_rcu()

/*
 *	Notifier list for kernel code which wants to be called
 *	at shutdown. This is used to stop any idling DMA operations
 *	and the like.
 */

/*
 *	Notifier chain core routines.  The exported routines below
 *	are layered on top of these, with appropriate locking added.
 */

static int notifier_chain_register(struct notifier_block **nl, struct notifier_block *n,
                                   bool unique_priority)
{
    while ((*nl) != NULL)
    {
        if (unlikely((*nl) == n))
        {
            WARN(1, "notifier callback %ps already registered", n->notifier_call);
            return -EEXIST;
        }
        if (n->priority > (*nl)->priority)
            break;
        if (n->priority == (*nl)->priority && unique_priority)
            return -EBUSY;
        nl = &((*nl)->next);
    }
    n->next = *nl;
    rcu_assign_pointer(*nl, n);
    trace_notifier_register((void *)n->notifier_call);
    return 0;
}

static int notifier_chain_unregister(struct notifier_block **nl, struct notifier_block *n)
{
    while ((*nl) != NULL)
    {
        if ((*nl) == n)
        {
            rcu_assign_pointer(*nl, n->next);
            trace_notifier_unregister((void *)n->notifier_call);
            return 0;
        }
        nl = &((*nl)->next);
    }
    return -ENOENT;
}

/**
 * notifier_call_chain - Informs the registered notifiers about an event.
 *	@nl:		Pointer to head of the blocking notifier chain
 *	@val:		Value passed unmodified to notifier function
 *	@v:		Pointer passed unmodified to notifier function
 *	@nr_to_call:	Number of notifier functions to be called. Don't care
 *			value of this parameter is -1.
 *	@nr_calls:	Records the number of notifications sent. Don't care
 *			value of this field is NULL.
 *	Return:		notifier_call_chain returns the value returned by the
 *			last notifier function called.
 */
static int notifier_call_chain(struct notifier_block **nl, unsigned long val, void *v,
                               int nr_to_call, int *nr_calls)
{
    int ret = NOTIFY_DONE;
    struct notifier_block *nb, *next_nb;

    nb = rcu_dereference_raw(*nl);

    while (nb && nr_to_call)
    {
        next_nb = rcu_dereference_raw(nb->next);

        trace_notifier_run((void *)nb->notifier_call);
        ret = nb->notifier_call(nb, val, v);

        if (nr_calls)
            (*nr_calls)++;

        if (ret & NOTIFY_STOP_MASK)
            break;
        nb = next_nb;
        nr_to_call--;
    }
    return ret;
}

/**
 * notifier_call_chain_robust - Inform the registered notifiers about an event
 *                              and rollback on error.
 * @nl:		Pointer to head of the blocking notifier chain
 * @val_up:	Value passed unmodified to the notifier function
 * @val_down:	Value passed unmodified to the notifier function when recovering
 *              from an error on @val_up
 * @v:		Pointer passed unmodified to the notifier function
 *
 * NOTE:	It is important the @nl chain doesn't change between the two
 *		invocations of notifier_call_chain() such that we visit the
 *		exact same notifier callbacks; this rules out any RCU usage.
 *
 * Return:	the return value of the @val_up call.
 */
static int notifier_call_chain_robust(struct notifier_block **nl, unsigned long val_up,
                                      unsigned long val_down, void *v)
{
    int ret, nr = 0;

    ret = notifier_call_chain(nl, val_up, v, -1, &nr);
    if (ret & NOTIFY_STOP_MASK)
        notifier_call_chain(nl, val_down, v, nr - 1, NULL);

    return ret;
}

/*
 *	Atomic notifier chain routines.  Registration and unregistration
 *	use a spinlock, and call_chain is synchronized by RCU (no locks).
 */

/**
 *	atomic_notifier_chain_register - Add notifier to an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to an atomic notifier chain.
 *
 *	Returns 0 on success, %-EEXIST on error.
 */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *n)
{
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&nh->lock, flags);
    ret = notifier_chain_register(&nh->head, n, false);
    spin_unlock_irqrestore(&nh->lock, flags);
    return ret;
}

/**
 *	atomic_notifier_chain_register_unique_prio - Add notifier to an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to an atomic notifier chain if there is no other
 *	notifier registered using the same priority.
 *
 *	Returns 0 on success, %-EEXIST or %-EBUSY on error.
 */
int atomic_notifier_chain_register_unique_prio(struct atomic_notifier_head *nh,
                                               struct notifier_block *n)
{
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&nh->lock, flags);
    ret = notifier_chain_register(&nh->head, n, true);
    spin_unlock_irqrestore(&nh->lock, flags);
    return ret;
}

/**
 *	atomic_notifier_chain_unregister - Remove notifier from an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from an atomic notifier chain.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh, struct notifier_block *n)
{
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&nh->lock, flags);
    ret = notifier_chain_unregister(&nh->head, n);
    spin_unlock_irqrestore(&nh->lock, flags);
    synchronize_rcu();
    return ret;
}

/**
 *	atomic_notifier_call_chain - Call functions in an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in an atomic context, so they must not block.
 *	This routine uses RCU to synchronize with changes to the chain.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then atomic_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 */
int atomic_notifier_call_chain(struct atomic_notifier_head *nh, unsigned long val, void *v)
{
    int ret;

    rcu_read_lock();
    ret = notifier_call_chain(&nh->head, val, v, -1, NULL);
    rcu_read_unlock();

    return ret;
}

/**
 *	atomic_notifier_call_chain_is_empty - Check whether notifier chain is empty
 *	@nh: Pointer to head of the atomic notifier chain
 *
 *	Checks whether notifier chain is empty.
 *
 *	Returns true is notifier chain is empty, false otherwise.
 */
bool atomic_notifier_call_chain_is_empty(struct atomic_notifier_head *nh)
{
    return !kernel_access_check(nh->head, sizeof(void *), UACCESS_R);
}

/*
 *	Raw notifier chain routines.  There is no protection;
 *	the caller must provide it.  Use at your own risk!
 */

/**
 *	raw_notifier_chain_register - Add notifier to a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to a raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Returns 0 on success, %-EEXIST on error.
 */
int raw_notifier_chain_register(struct raw_notifier_head *nh, struct notifier_block *n)
{
    return notifier_chain_register(&nh->head, n, false);
}

/**
 *	raw_notifier_chain_unregister - Remove notifier from a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from a raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int raw_notifier_chain_unregister(struct raw_notifier_head *nh, struct notifier_block *n)
{
    return notifier_chain_unregister(&nh->head, n);
}

int raw_notifier_call_chain_robust(struct raw_notifier_head *nh, unsigned long val_up,
                                   unsigned long val_down, void *v)
{
    return notifier_call_chain_robust(&nh->head, val_up, val_down, v);
}

/**
 *	raw_notifier_call_chain - Call functions in a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in an undefined context.
 *	All locking must be provided by the caller.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then raw_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 */
int raw_notifier_call_chain(struct raw_notifier_head *nh, unsigned long val, void *v)
{
    return notifier_call_chain(&nh->head, val, v, -1, NULL);
}
