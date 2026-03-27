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

#include <process_signal.h>
#include <klog.h>

/* 信号集合操作 */
void signal_set_or(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1)
{
    switch (K_SIGSET_NLONG)
    {
    case 4:
        dset->sig[3] = set0->sig[3] | set1->sig[3];
        dset->sig[2] = set0->sig[2] | set1->sig[2];
        /* fall through: 继续处理更低位的 word。 */
    case 2:
        dset->sig[1] = set0->sig[1] | set1->sig[1];
        /* fall through: case 1 始终负责第 0 个 word。 */
    case 1:
        dset->sig[0] = set0->sig[0] | set1->sig[0];
        break;
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        break;
    }
}

void signal_set_not(k_sigset_t *dset, const k_sigset_t *set)
{
    switch (K_SIGSET_NLONG)
    {
    case 4:
        dset->sig[3] = ~set->sig[3];
        dset->sig[2] = ~set->sig[2];
        /* fall through: 继续处理更低位的 word。 */
    case 2:
        dset->sig[1] = ~set->sig[1];
        /* fall through: case 1 始终负责第 0 个 word。 */
    case 1:
        dset->sig[0] = ~set->sig[0];
        break;
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        return;
    }
}

bool signal_set_is_empty(k_sigset_t *set)
{
    if (set == NULL)
    {
        return true;
    }

    switch (K_SIGSET_NLONG)
    {
    case 4:
        return (set->sig[3] | set->sig[2] | set->sig[1] | set->sig[0]) == 0;
    case 2:
        return (set->sig[1] | set->sig[0]) == 0;
    case 1:
        return set->sig[0] == 0;
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        return false;
    }
}

void signal_set_and(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1)
{
    switch (K_SIGSET_NLONG)
    {
    case 4:
        dset->sig[3] = set0->sig[3] & set1->sig[3];
        dset->sig[2] = set0->sig[2] & set1->sig[2];
        /* fall through: 继续处理更低位的 word。 */
    case 2:
        dset->sig[1] = set0->sig[1] & set1->sig[1];
        /* fall through: case 1 始终负责第 0 个 word。 */
    case 1:
        dset->sig[0] = set0->sig[0] & set1->sig[0];
        break;
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        return;
    }
}

void signal_set_and_not(k_sigset_t *dset, const k_sigset_t *set0, const k_sigset_t *set1)
{
    switch (K_SIGSET_NLONG)
    {
    case 4:
        dset->sig[3] = set0->sig[3] & ~set1->sig[3];
        dset->sig[2] = set0->sig[2] & ~set1->sig[2];
        /* fall through: 继续处理更低位的 word。 */
    case 2:
        dset->sig[1] = set0->sig[1] & ~set1->sig[1];
        /* fall through: case 1 始终负责第 0 个 word。 */
    case 1:
        dset->sig[0] = set0->sig[0] & ~set1->sig[0];
        break;
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        return;
    }
}

bool signal_set_contains(const k_sigset_t *set, int sig_num)
{
    if (sig_num <= 0 || sig_num > SIGNAL_MAX)
    {
        return false;
    }

    if (K_SIGSET_NLONG == 1)
    {
        return !!(set->sig[0] & sigmask(sig_num));
    }
    else
    {
        return !!(set->sig[SIGSET_INDEX(sig_num - 1)] & (1UL << SIGSET_BIT(sig_num - 1)));
    }
}

bool signal_set_equal(const k_sigset_t *set1, const k_sigset_t *set2)
{
    switch (K_SIGSET_NLONG)
    {
    case 4:
        return (set1->sig[3] == set2->sig[3]) && (set1->sig[2] == set2->sig[2]) &&
               (set1->sig[1] == set2->sig[1]) && (set1->sig[0] == set2->sig[0]);
    case 2:
        return (set1->sig[1] == set2->sig[1]) && (set1->sig[0] == set2->sig[0]);
    case 1:
        return set1->sig[0] == set2->sig[0];
    default:
        KLOG_EMERG("%s: unsupported K_SIGSET_NLONG=%d", __func__, K_SIGSET_NLONG);
        return false;
    }
}

void signal_set_add(k_sigset_t *set, int sig_num)
{
    if (K_SIGSET_NLONG == 1)
    {
        set->sig[0] |= sigmask(sig_num);
    }
    else
    {
        set->sig[SIGSET_INDEX(sig_num - 1)] |= 1UL << SIGSET_BIT(sig_num - 1);
    }
}

void signal_set_del(k_sigset_t *set, int sig_num)
{
    if (K_SIGSET_NLONG == 1)
    {
        set->sig[0] &= ~sigmask(sig_num);
    }
    else
    {
        set->sig[SIGSET_INDEX(sig_num - 1)] &= ~(1UL << SIGSET_BIT(sig_num - 1));
    }
}

void signal_set_clear(k_sigset_t *set)
{
    memset(set, 0, sizeof(*set));
}

k_sigset_t signal_set_from_long_mask(unsigned long mask)
{
    k_sigset_t set = {0};

    set.sig[0] = mask;
    return set;
}

void signal_set_del_immutable(k_sigset_t *set)
{
    signal_set_del(set, SIGKILL);
    signal_set_del(set, SIGSTOP);
}
