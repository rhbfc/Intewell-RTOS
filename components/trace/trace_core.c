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

#include <fcntl.h>
#include <fs/fs.h>
#include <fs/procfs.h>
#include <fs/stat.h>
#include <system/compiler.h>
#include <system/kconfig.h>
#include <trace.h>
#include <ttos.h>
#include <ttos_init.h>

#ifdef TRACE_SLOT_SUPPORT
#define TRACE_SLOT_NUM 4

/* 多slot实现 */
#define TRACE_FUNC(name, _ops, param, arg)                                                         \
    void name param                                                                                \
    {                                                                                              \
        for (uint32_t i = 0; i < TRACE_SLOT_NUM; i++)                                              \
        {                                                                                          \
            struct trace_slot *slot = &traces[i];                                                  \
            if (slot && slot->used)                                                                \
            {                                                                                      \
                slot->ops->_ops arg;                                                               \
            }                                                                                      \
        }                                                                                          \
    }
#else
#define TRACE_SLOT_NUM 1

/* 单slot优化实现 */
#define TRACE_FUNC(name, _ops, param, arg)                                                         \
    void name param                                                                                \
    {                                                                                              \
        if (traces && traces->used)                                                                \
        {                                                                                          \
            traces->ops->_ops arg;                                                                 \
        }                                                                                          \
    }
#endif

uint64_t __ttos_int_disable_start[CONFIG_MAX_CPUS];
uint32_t __ttos_int_lock_depth[CONFIG_MAX_CPUS];
struct trace_slot traces[TRACE_SLOT_NUM];

TRACE_FUNC(_trace_object, object, (uint32_t type, void *data), (type, data));
TRACE_FUNC(_trace_task_bindcpu, task_bindcpu, (void *task), (task));
TRACE_FUNC(_trace_switch_task, switch_task, (void *old_task, void *new_task), (old_task, new_task));
TRACE_FUNC(_trace_waking_task, waking_task, (void *task), (task));
TRACE_FUNC(_trace_mark_begin, mark, (const void *point), (point, 1));
TRACE_FUNC(_trace_mark_end, mark, (const void *point), (point, 0));
TRACE_FUNC(_trace_irq, irq, (uint32_t virq, uint64_t stimestamp, uint32_t duration),
           (virq, stimestamp, duration));
TRACE_FUNC(_trace_syscall, syscall, (uint32_t id, uint32_t is_entry, void *data),
           (id, is_entry, data));
TRACE_FUNC(_trace_log, log, (uint32_t level, char *tag, char *msg, uint32_t len),
           (level, tag, msg, len));
TRACE_FUNC(_trace_signal_generate, signal_generate,
           (int32_t code, uint32_t pgid, uint32_t pid, int32_t sig, int32_t result),
           (code, pgid, pid, sig, result));
TRACE_FUNC(_trace_signal_deliver, signal_deliver, (int32_t code, int32_t sig, int32_t sa_flags),
           (code, sig, sa_flags));
TRACE_FUNC(_trace_malloc, malloc,
           (uint32_t alloc_size, uint32_t req_size, uint32_t flag, void *call_site, void *ptr),
           (alloc_size, req_size, flag, call_site, ptr));
TRACE_FUNC(_trace_free, free, (void *call_site, void *ptr), (call_site, ptr));
TRACE_FUNC(_trace_exec, exec, (char *filename), (filename));
TRACE_FUNC(_trace_fork, fork, (void *child_task), (child_task));
TRACE_FUNC(_trace_exit, exit, (void), ());
TRACE_FUNC(_trace_compete, compete, (void *call_site, uint32_t type, uint32_t duration),
           (call_site, type, duration));

static void ops_null() {}

#define OPS_BIT(b, f)                                                                              \
    {                                                                                              \
        BIT(b), offsetof(struct trace_ops, f)                                                      \
    }

struct ops_func_bit
{
    uint32_t bit;
    uint32_t ops_offset;
};

static const struct ops_func_bit _ops_func_bit[] = {
    OPS_BIT(1, mark),
    OPS_BIT(2, irq),
    OPS_BIT(3, syscall),
    OPS_BIT(4, log),
    OPS_BIT(5, signal_generate),
    OPS_BIT(5, signal_deliver),
    OPS_BIT(6, malloc),
    OPS_BIT(6, free),
    OPS_BIT(7, compete),
    {0},
};

static void trace_update_ops_by_bit(struct trace_slot *slot)
{
    uint32_t ops_bit = slot->ops_bit;
    uint32_t used;
    struct trace_ops *ops = slot->ops;
    struct trace_ops *bops = slot->back_ops;

    /* 遍历映射表，逐字段处理 */
    const struct ops_func_bit *p = _ops_func_bit;
    for (; p->bit != 0; p++)
    {
        void (**fn)(void) = (void (**)(void))((char *)ops + p->ops_offset);
        void (**bfn)(void) = (void (**)(void))((char *)bops + p->ops_offset);

        if (ops_bit & p->bit)
        {
            /* bit = 1 → 恢复原始处理函数 */
            *fn = *bfn;
        }
        else
        {
            /* bit = 0 → 屏蔽此 ops 项 */
            *fn = ops_null;
        }
    }

    used = !!(ops_bit & BIT(0));

    if (slot->used ^ used)
    {
        slot->used = used;
        slot->ops->config(slot);
    }
}

static void trace_get_ops_bit(struct trace_slot *slot)
{
    uint32_t ops_bit = 0;
    struct trace_ops *ops = slot->ops;

    for (int i = 0; _ops_func_bit[i].bit != 0; i++)
    {
        uint32_t bit = _ops_func_bit[i].bit;
        void *fn = (void *)ops + _ops_func_bit[i].ops_offset;

        if (fn != ops_null)
            ops_bit |= bit;
    }

    slot->ops_bit = ops_bit | (slot->used ? 1 : 0);
}

struct seq_data
{
    char buf[64];
    struct proc_seq_buf seq;
};

static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct trace_slot *slot = inode->data;
    struct seq_data *seqd = slot->seq_data;

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&seqd->seq, seqd->buf, sizeof(seqd->buf), ppos);
        proc_seq_printf(&seqd->seq, "0x%X\n", slot->ops_bit);
    }

    return proc_seq_read(&seqd->seq, buffer, count);
}

static ssize_t proc_file_write(struct proc_dir_entry *inode, void *priv, const char *buffer,
                               size_t count, off_t *ppos)
{
    struct trace_slot *slot = inode->data;
    struct seq_data *seqd = slot->seq_data;
    slot->ops_bit = strtoul(buffer, NULL, 0);
    trace_update_ops_by_bit(slot);
    return count;
}

static struct proc_ops proc_ops = {
    .proc_read = proc_file_read,
    .proc_write = proc_file_write,
};

static struct trace_slot *trace_slot_alloc(struct trace_ops *ops)
{
    if (!ops->switch_task)
        return NULL;

    size_t n = sizeof(struct trace_ops) / sizeof(void *);
    void **p = (void **)ops;

    for (size_t i = 0; i < n; i++)
    {
        if (!p[i])
            p[i] = ops_null;
    }

    for (size_t i = 0; i < TRACE_SLOT_NUM; i++)
    {
        if (traces[i].used == false)
        {
            traces[i].ops = ops;
            traces[i].back_ops = ops;
            traces[i].seq_data = malloc(sizeof(struct seq_data));
            return &traces[i];
        }
    }

    return NULL;
}

int trace_register(const char *name, struct trace_ops *ops)
{
    if (!IS_ENABLED(CONFIG_TRACE))
        return -1;

    struct trace_slot *slot;
    struct proc_dir_entry *entry;
    static struct proc_dir_entry *trace_dir;

    slot = trace_slot_alloc(ops);

    if (!slot)
    {
        printk("error, no free slot.\n");
        return -1;
    }

    if (slot->ops->init)
        slot->ops->init(slot);

    trace_get_ops_bit(slot);

    if (!trace_dir)
        trace_dir = proc_mkdir("trace", NULL);

    entry = proc_create(name, 0666, trace_dir, &proc_ops);
    entry->data = slot;

    return 0;
}

static int trace_info_init(void)
{
    mkdir("/etc/trace", 0755);
    struct file f;
    int ret = file_open(&f, "/etc/trace/info", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (ret < 0)
    {
        return ret;
    }
    const char *trace_options =
        "[0] TRACE_ENABLE   - Enable tracing\n"
        "[1] MARK           - Trace mark begin / end points for performance measurement.\n"
        "[2] IRQ            - IRQ events\n"
        "[3] SYSCALL        - System calls\n"
        "[4] LOG            - Log messages\n"
        "[5] SIGNAL         - Signals\n"
        "[6] MEMORY         - Memory operations\n"
        "[7] COMPETE        - Compete events(INT / spinlock)\n";

    file_write(&f, trace_options, strlen(trace_options));
    file_close(&f);

    return 0;
}
INIT_EXPORT_SERVE_FS(trace_info_init, "trace_info_init");