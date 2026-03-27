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

#include <assert.h>
#include <atomic.h>
#include <errno.h>
#include <limits.h>
#include <signal_internal.h>
#include <spinlock.h>
#include <time/posix_timer.h>
#include <ttosProcess.h>
#include <ttosRBTree.h>
#include <uaccess.h>

struct posix_timer *posix_timer_alloc(void)
{
    struct posix_timer *posix_timer = calloc(1, sizeof(struct posix_timer));

    if (!posix_timer)
    {
        return NULL;
    }

    posix_timer->sig_info_ptr = calloc(1, sizeof(*posix_timer->sig_info_ptr));
    if (!posix_timer->sig_info_ptr)
    {
        free(posix_timer);
        return NULL;
    }

    INIT_LIST_HEAD(&posix_timer->node);

    rb_init_node(&posix_timer->posix_timer_rb_node);

    posix_timer->sig_info_ptr->priv = (void *)posix_timer;
    atomic_set(&posix_timer->refcnt, 1);
    atomic_set(&posix_timer->posix_timer_deleted, 0);

    return posix_timer;
}

static int posix_timer_prepare_siginfo(struct posix_timer *timer)
{
    if (!timer)
    {
        return -EINVAL;
    }

    if (timer->sig_info_ptr)
    {
        return 0;
    }

    timer->sig_info_ptr = calloc(1, sizeof(*timer->sig_info_ptr));
    if (!timer->sig_info_ptr)
    {
        return -EAGAIN;
    }

    timer->sig_info_ptr->priv = timer;
    return 0;
}

void posix_timer_free(struct posix_timer *timer)
{
    if (!timer)
    {
        return;
    }

    free(timer->sig_info_ptr);
    timer->sig_info_ptr = NULL;

    free(timer);
}

static void posix_timer_handle_dropped_notification(struct posix_timer *timer)
{
    /*
     * 只有“当前这一代”已进入 signal pending 的通知被丢弃时，
     * 才需要把状态收口回 STOPPED。
     *
     * 如果 timer 已经被重新 set/delete，signal_seq 会先行变化，
     * 旧通知后续被丢弃时不能反向覆盖新的运行态。
     */
    if (!timer)
    {
        return;
    }

    if (timer->state == POSIX_TIMER_SIGNAL_PENDING &&
        timer->posix_timer_signal_seq == timer->posix_timer_sigqueue_seq)
    {
        timer->state = POSIX_TIMER_STOPPED;
    }
}

void posix_timer_signal_drop(void *priv)
{
    struct posix_timer *timer = (struct posix_timer *)priv;

    /*
     * POSIX timer 在回调发信号前会额外持有一份引用。
     * 如果该信号没有走到正常的 signal delivery 路径，就需要在丢弃点显式归还。
     */
    if (timer)
    {
        posix_timer_handle_dropped_notification(timer);
        posix_timer_putref(timer);
    }
}

void posix_timer_getref(struct posix_timer *timer)
{
    if (!timer)
    {
        return;
    }

    atomic_inc(&timer->refcnt);
}

int posix_timer_putref(struct posix_timer *timer)
{
    if (!timer)
    {
        return -1;
    }

    if (atomic_dec_return(&timer->refcnt) == 0)
    {
        posix_timer_free(timer);
        return 0;
    }

    return 1;
}

bool posix_timer_is_deleted(struct posix_timer *timer)
{
    if (!timer)
    {
        return true;
    }

    return atomic_read(&timer->posix_timer_deleted) ? true : false;
}

void posix_timer_signal_lock(struct posix_timer *timer, irq_flags_t *irq_flag)
{
    irq_flags_t flag;
    struct shared_signal *signal;

    assert(!!timer);
    assert(!!timer->posix_timer_signal);

    signal = timer->posix_timer_signal;
    spin_lock_irqsave(&signal->siglock, flag);

    *irq_flag = flag;
}

void posix_timer_signal_unlock(struct posix_timer *timer, irq_flags_t *irq_flag)
{
    irq_flags_t flag;
    struct shared_signal *signal;

    assert(!!timer);
    assert(!!timer->posix_timer_signal);

    flag = *irq_flag;
    signal = timer->posix_timer_signal;
    spin_unlock_irqrestore(&signal->siglock, flag);
}

static inline void posix_timer_mark_deleted(struct posix_timer *timer)
{
    if (!timer)
    {
        return;
    }

    atomic_write(&timer->posix_timer_deleted, 1);
    timer->posix_timer_signal_seq++;
}

static struct posix_timer *posix_timer_id_rb_search(struct rb_root *root, int id)
{
    struct rb_node *node = root->rb_node;

    while (node)
    {
        struct posix_timer *this = rb_entry(node, struct posix_timer, posix_timer_rb_node);

        if (id < this->posix_timer_id)
        {
            node = node->rb_left;
        }
        else if (id > this->posix_timer_id)
        {
            node = node->rb_right;
        }
        else
        {
            return this;
        }
    }

    return NULL;
}

static int posix_timer_id_rb_insert(struct rb_root *root, struct posix_timer *timer)
{
    struct rb_node **new_node = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*new_node)
    {
        struct posix_timer *this = rb_entry(*new_node, struct posix_timer, posix_timer_rb_node);
        parent = *new_node;

        if (timer->posix_timer_id < this->posix_timer_id)
        {
            new_node = &(*new_node)->rb_left;
        }
        else if (timer->posix_timer_id > this->posix_timer_id)
        {
            new_node = &(*new_node)->rb_right;
        }
        else
        {
            return -EEXIST;
        }
    }

    rb_link_node(&timer->posix_timer_rb_node, parent, new_node);
    rb_insert_color(&timer->posix_timer_rb_node, root);
    return 0;
}

static void posix_timer_id_rb_erase(struct rb_root *root, struct posix_timer *timer)
{
    if (RB_EMPTY_NODE(&timer->posix_timer_rb_node))
    {
        return;
    }

    rb_erase(&timer->posix_timer_rb_node, root);
    RB_CLEAR_NODE(&timer->posix_timer_rb_node);
}

static int posix_timer_id_alloc(struct posix_timer *timer, int default_id)
{
    int ret = 0;
    unsigned long flags;
    int id = default_id;
    pcb_t pcb = ttosProcessSelf();
    struct shared_signal *sig = signal_shared_get(pcb);

    if (!sig)
    {
        return -EINVAL;
    }

    timer->posix_timer_signal = sig;

    signal_lock(pcb, &flags);

    /* 不等，表示使用用户传入的id,而非动态分配 */
    if (default_id != TIMER_ANY_ID)
    {
        timer->posix_timer_id = id;
        if (posix_timer_id_rb_search(sig->posix_timer_root, id))
        {
            signal_unlock(pcb, &flags);
            return -EEXIST;
        }

        ret = posix_timer_id_rb_insert(sig->posix_timer_root, timer);
        if (!ret)
        {
            atomic_set(&sig->next_posix_timer_id, id + 1);
        }
        signal_unlock(pcb, &flags);
        return ret ? ret : id;
    }

    /* 动态分配id */
    for (unsigned int cnt = 0; cnt <= INT_MAX; cnt++)
    {
        /* 原子获取一个timer id*/
        id = (int)(atomic_fetch_add(&sig->next_posix_timer_id, 1) & INT_MAX);

        /* 查找id是否存在 */
        if (!posix_timer_id_rb_search(sig->posix_timer_root, id))
        {
            /* id是否不存在，则使用该id */
            timer->posix_timer_id = id;
            ret = posix_timer_id_rb_insert(sig->posix_timer_root, timer);
            if (!ret)
            {
                signal_unlock(pcb, &flags);
                return id;
            }
        }
    }

    signal_unlock(pcb, &flags);
    return -EAGAIN;
}

void posix_timer_delete(struct posix_timer *timer)
{
    struct shared_signal *sig;
    irq_flags_t flags;

    if (!timer)
    {
        return;
    }

    sig = timer->posix_timer_signal;
    if (sig)
    {
        signal_lock(timer->pcb, &flags);
        posix_timer_mark_deleted(timer);
        posix_timer_id_rb_erase(sig->posix_timer_root, timer);
        signal_unlock(timer->pcb, &flags);
    }
    else
    {
        posix_timer_mark_deleted(timer);
    }
}

struct posix_timer *posix_timer_get(int timer_id)
{
    pcb_t pcb = ttosProcessSelf();
    struct shared_signal *sig = signal_shared_get(pcb);
    struct posix_timer *timer = NULL;
    irq_flags_t flags;

    if (!sig || timer_id < 0)
    {
        return NULL;
    }

    signal_lock(pcb, &flags);
    timer = posix_timer_id_rb_search(sig->posix_timer_root, timer_id);
    if (timer && !posix_timer_is_deleted(timer))
    {
        posix_timer_getref(timer);
    }
    else
    {
        timer = NULL;
    }
    signal_unlock(pcb, &flags);

    return timer;
}

void posix_timer_clean_shared(struct shared_signal *sig)
{
    struct rb_node *node;
    struct rb_node *next;
    struct posix_timer *timer;
    struct posix_timer *timer_tmp;
    LIST_HEAD(to_free);
    unsigned long flags;

    if (!sig)
    {
        return;
    }

    /*
     * shared_signal 是 POSIX timer 树的真正宿主。
     * 这里直接锁住 shared_signal，而不是依赖 current pcb，方便在对象最终析构时复用。
     */
    spin_lock_irqsave(&sig->siglock, flags);
    node = rb_first(sig->posix_timer_root);
    while (node)
    {
        next = rb_next(node);
        timer = rb_entry(node, struct posix_timer, posix_timer_rb_node);
        posix_timer_mark_deleted(timer);
        posix_timer_id_rb_erase(sig->posix_timer_root, timer);

        /* 加入临时节点 */
        list_add_tail(&timer->node, &to_free);

        node = next;
    }
    spin_unlock_irqrestore(&sig->siglock, flags);

    /* 处理临时节点，释放资源 */
    list_for_each_entry_safe(timer, timer_tmp, &to_free, node)
    {
        if (timer->clock_ops)
        {
            /* 取消posix-timer */
            while (timer->clock_ops->remove_timer(timer) == TIMER_RETRY)
            {
            }
        }

        /* 释放posix-timer */
        posix_timer_putref(timer);
    }
}

void posix_timer_clean(void)
{
    pcb_t pcb = ttosProcessSelf();

    if (!pcb)
    {
        return;
    }

    posix_timer_clean_shared(signal_shared_get(pcb));
}

static int posix_timer_id_init(timer_t __user *created_timer_id)
{
    int id = TIMER_ANY_ID;
    struct shared_signal *signal = signal_shared_get(ttosProcessSelf());

    /* 使用用户传入的id */
    if (unlikely(signal->assign_timer_id))
    {
        if (copy_from_user(&id, created_timer_id, sizeof(id)))
        {
            return -EFAULT;
        }
    }

    return id;
}

static int posix_timer_resolve_target(sigevent_t *event, pcb_t owner_pcb, pid_t *notify_tid)
{
    pcb_t pcb_tmp;
    TASK_ID task;

    if (notify_tid)
    {
        *notify_tid = 0;
    }

    switch (event->sigev_notify)
    {
    case SIGEV_SIGNAL | SIGEV_THREAD_ID:
        task = task_get_by_tid(event->_sigev_un._tid);
        if (task == NULL)
        {
            return -EINVAL;
        }

        pcb_tmp = task->ppcb;
        if (pcb_tmp == NULL)
        {
            return -EINVAL;
        }
        if (pcb_tmp->group_leader != owner_pcb->group_leader)
        {
            return -EINVAL;
        }

        /*
         * SIGEV_THREAD_ID 只需要在创建时校验“目标线程属于同一线程组”。
         * 真正发信号时只使用 tid，避免长期持有目标线程 pcb 导致退出后的悬空指针。
         */
        if (notify_tid)
        {
            *notify_tid = task->tid;
        }

        fallthrough;
    case SIGEV_SIGNAL:
    case SIGEV_THREAD:
        if (event->sigev_signo <= 0 || event->sigev_signo > SIGRTMAX)
            return -EINVAL;
        fallthrough;
    case SIGEV_NONE:
        return 0;
    default:
        return -EINVAL;
    }
}

int posix_timer_create(clockid_t which_clock, sigevent_t *event, timer_t __user *created_timer_id)
{
    const struct posix_clock_ops *clock_ops = clock_id_to_ops(which_clock);
    struct posix_timer *new_timer;
    pcb_t owner_pcb = ttosProcessSelf()->group_leader;
    int error = 0;
    int timer_id = 0;
    int default_id;
    irq_flags_t irq_flag;

    if (!clock_ops)
    {
        return -EINVAL;
    }

    if (!clock_ops->create_timer)
    {
        return -EOPNOTSUPP;
    }

    new_timer = posix_timer_alloc();
    if (!new_timer)
    {
        return -EAGAIN;
    }

    default_id = posix_timer_id_init(created_timer_id);
    if (-EFAULT == default_id)
    {
        posix_timer_putref(new_timer);
        return default_id;
    }

    timer_id = posix_timer_id_alloc(new_timer, default_id);
    if (timer_id < 0)
    {
        posix_timer_putref(new_timer);
        return timer_id;
    }

    new_timer->posix_timer_clock = which_clock;
    new_timer->clock_ops = clock_ops;
    new_timer->posix_timer_overrun = -1LL;
    new_timer->pcb = owner_pcb;
    new_timer->notify_tid = 0;

    error = posix_timer_prepare_siginfo(new_timer);
    if (error)
    {
        goto out;
    }

    if (event)
    {
        error = posix_timer_resolve_target(event, owner_pcb, &new_timer->notify_tid);
        if (error)
        {
            goto out;
        }

        new_timer->posix_timer_sigev_notify = event->sigev_notify;
        new_timer->sig_info_ptr->ksiginfo.si_signo = event->sigev_signo;
        new_timer->sig_info_ptr->ksiginfo.si_value = event->sigev_value;
    }
    else
    {
        new_timer->posix_timer_sigev_notify = SIGEV_SIGNAL;
        new_timer->sig_info_ptr->ksiginfo.si_signo = SIGALRM;
        new_timer->sig_info_ptr->ksiginfo.si_value.sival_int = new_timer->posix_timer_id;
    }

    if (new_timer->posix_timer_sigev_notify & SIGEV_THREAD_ID)
    {
        new_timer->signal_to_thread = true;
    }
    else
    {
        new_timer->signal_to_thread = false;
    }

    new_timer->sig_info_ptr->ksiginfo.si_timerid = new_timer->posix_timer_id;
    new_timer->sig_info_ptr->ksiginfo.si_code = SI_TIMER;

    if (copy_to_user((void *)created_timer_id, (void *)&timer_id, sizeof(timer_id)))
    {
        error = -EFAULT;
        goto out;
    }

    error = clock_ops->create_timer(new_timer);
    if (error)
        goto out;

    signal_lock(new_timer->pcb, &irq_flag);
    WRITE_ONCE(new_timer->posix_timer_signal, signal_shared_get(new_timer->pcb));
    signal_unlock(new_timer->pcb, &irq_flag);

    return 0;
out:
    posix_timer_delete(new_timer);
    posix_timer_putref(new_timer);
    return error;
}

/* 先查找，再删除 */
struct posix_timer *posix_timer_detach(int timer_id)
{
    pcb_t pcb = ttosProcessSelf();
    struct shared_signal *sig = signal_shared_get(pcb);
    struct posix_timer *timer = NULL;
    unsigned long flags;

    if (!sig || timer_id < 0)
    {
        return NULL;
    }

    signal_lock(pcb, &flags);
    timer = posix_timer_id_rb_search(sig->posix_timer_root, timer_id);
    if (timer)
    {
        posix_timer_mark_deleted(timer);
        posix_timer_id_rb_erase(sig->posix_timer_root, timer);
    }
    signal_unlock(pcb, &flags);

    return timer;
}

bool posixtimer_signal_process(ksiginfo_entry_t *siginfo)
{
    struct posix_timer *posix_timer = (struct posix_timer *)siginfo->priv;

    if (!posix_timer)
    {
        return false;
    }

    if (posix_timer_is_deleted(posix_timer))
    {
        posix_timer_putref(posix_timer);
        return false;
    }

    /* 不等，表示timer从发出信号到现这个过程中，被重新设置/删除过 */
    if (posix_timer->posix_timer_signal_seq != posix_timer->posix_timer_sigqueue_seq)
    {
        posix_timer_putref(posix_timer);
        /* 丢弃本次timer信号，不设置下一次触发 */
        return false;
    }

    /* it_interval为0，表示不触发下一次 */
    if (!posix_timer->posix_timer_interval ||
        posix_timer->state != POSIX_TIMER_SIGNAL_PENDING)
    {
        posix_timer_putref(posix_timer);
        /* 投递本次信号，不设置下一次触发 */
        return true;
    }

    /* 继续设置下一次触发 */
    posix_timer->clock_ops->rearm_timer(posix_timer);
    posix_timer->state = POSIX_TIMER_RUNNING;
    posix_timer->posix_timer_overrun_last = posix_timer->posix_timer_overrun;
    posix_timer->posix_timer_overrun = -1LL; // ktimer_forward默认会返回1
    ++posix_timer->posix_timer_signal_seq;
    siginfo->ksiginfo.si_overrun = posix_timer->posix_timer_overrun_last > (s64)INT_MAX
                                       ? INT_MAX
                                       : posix_timer->posix_timer_overrun_last;

    if (!posix_timer_putref(posix_timer))
    {
        return false;
    }

    return true;
}

int copy_timespec64_to_user(const struct timespec64 *src, struct timespec64 __user *dest)
{
    struct timespec64 kts = {.tv_sec = src->tv_sec, .tv_nsec = src->tv_nsec};

    return copy_to_user(dest, &kts, sizeof(kts)) ? -EFAULT : 0;
}

int copy_itimerspec64_to_user(const struct itimerspec64 *src, struct itimerspec64 __user *dest)
{
    int ret;

    ret = copy_timespec64_to_user(&src->it_interval, &dest->it_interval);
    if (ret)
        return ret;

    ret = copy_timespec64_to_user(&src->it_value, &dest->it_value);

    return ret;
}

int copy_timespec64_from_user(struct timespec64 *dest, const struct timespec64 __user *src)
{
    struct timespec64 kts;
    int ret;

    ret = copy_from_user(&kts, src, sizeof(kts));
    if (ret)
        return -EFAULT;

    dest->tv_sec = kts.tv_sec;

    /* In 32-bit mode, this drops the padding */
    dest->tv_nsec = kts.tv_nsec;

    return 0;
}

int copy_itimerspec64_from_user(struct itimerspec64 *dest, const struct itimerspec64 __user *src)
{
    int ret;

    ret = copy_timespec64_from_user(&dest->it_interval, &src->it_interval);
    if (ret)
        return ret;

    ret = copy_timespec64_from_user(&dest->it_value, &src->it_value);

    return ret;
}
