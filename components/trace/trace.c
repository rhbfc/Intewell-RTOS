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

#include <cpuid.h>
#include <fcntl.h>
#include <mpscq.h>
#include <syscalls.h>
#include <system/kconfig.h>
#include <time/ktime.h>
#include <trace.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#include <version.h>
#include <wqueue.h>
#include <ttos_time.h>

struct trace_user
{
    struct trace_slot *slot;
    struct mpscq mpscq;
    struct work_s work;
    bool filesystem_ready;
    bool work_ready;
    struct file f_stored;
    struct file *f;
    uint8_t *data_cache;
    size_t cache_sz;
    size_t cache_offset;
};

enum trace_type
{
    TRACE_TYPE_SWITCH_THREAD = 1,
    TRACE_TYPE_MARK,
    TRACE_TYPE_IRQ,
    TRACE_TYPE_SYSCALL,
    TRACE_TYPE_SIG,
    TRACE_TYPE_LOG,
    TRACE_TYPE_OBJECT,
    TRACE_TYPE_SYSTEM_INFO,
    TRACE_TYPE_SCHED_WAKING,
    TRACE_TYPE_TASK_BIND_CPU,
    TRACE_TYPE_MALLOC_FREE,
    TRACE_TYPE_PROCESS_STS,
    TRACE_TYPE_COMPETE,
};

#pragma pack(push, 4)
/* 线程切换 */
struct trace_switch_task_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t cpu;
    uint8_t prev_prio;
    uint8_t next_prio;
    uint8_t res[3];
    uint64_t timestamp;
    uint32_t prev_state;
    uint32_t prev_tid;
    uint32_t next_tid;
};

struct trace_task_bind_cpu_entry
{
    uint8_t type;
    uint8_t length;
    uint16_t bind_cpu;
    uint32_t tid;
};

struct trace_waking_task_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t prio;
    uint8_t cpu;
    uint32_t tid;
    uint64_t timestamp;
};

/* 函数点标记 */
struct trace_mark_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t flag;
    uint8_t cpu;
    uint64_t timestamp;
    char *func;
};

/* 中断标记 */
struct trace_irq_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t res[1];
    uint8_t cpu;
    uint32_t irq;
    uint32_t duration; /* 中断持续时间 */
    uint64_t timestamp;
};

/* syscall */
struct trace_syscall_entry
{
    uint8_t type; /* syscall_entry, syscall_exit */
    uint8_t length;
    uint8_t cpu;
    uint8_t argc;
    uint16_t sts; /* bit0: entry / exit, bit1 64/32bit */
    uint16_t id;  /* syscall id */
    uint64_t timestamp;
    unsigned long args[0]; /* args / ret */
};

struct trace_sig_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t cpu;
    uint8_t res;
    int32_t sig;
    int32_t code;
    int32_t sa_flags_ret;
    uint64_t timestamp;
    uint32_t pgid; /*  pgid */
    uint32_t pid;  /*  pid */
};

struct trace_process_sts_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t cpu;
    uint8_t ps_type;
    uint32_t args_pid;
    uint64_t timestamp;
    char *filename;
};

struct trace_log_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t level;
    uint8_t cpu;
    uint64_t timestamp;
    char *tag;
    char *msg;
};

struct trace_malloc_free_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t cpu;
    uint8_t flag;
    union {
        uint32_t mflag;
        uint8_t res[4];
    };
    uint64_t timestamp;
    void *call_site;
    void *ptr;
    uint32_t alloc_size;
    uint32_t req_size;
};

struct trace_compete_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t cpu;
    uint8_t compete_type;
    uint32_t duration;
    uint64_t timestamp;
    void *call_site;
};

/*  对象声明 */
struct trace_object_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t objtype; /* 对象类型 */
    union {
        uint8_t res;
        uint8_t bind_cpu;
    };
    union {
        uint32_t uuid; /*  kthread  tid
                           thread   tgid + tid
                           proess   ppid + pid*/
        uint32_t irq;
        uint32_t strid; /* 字符串ID */
    };
    char *name;   /* 对象名称 */
    char *cmdstr; /* 命令行字符串 */
};

struct trace_system_info_entry
{
    uint8_t type;
    uint8_t length;
    uint8_t res[2];
    uint32_t clock_freq;
    uint64_t timestamp;
    char *sysname;
    char *release;
    char *machine;
    char *version;
};
#pragma pack(pop)

#define KTHREAD_UUID(tid) tid
#define THREAD_UUID(tgid, tid) ((tgid) << 17) | tid
#define PROESS_UUID(ppid, pid) ((ppid) << 17) | pid

#define TRACE_ELEM_SIZE 32
#define TRACE_ELEM_NUM 2 * 1024 * 1024
#define TRACE_MQ_NUM(len) ((len) + TRACE_ELEM_SIZE - 1) / TRACE_ELEM_SIZE

T_UBYTE corePriorityToPthread(int priority);

static struct trace_user wrok_trace;

#define ENTRY_INIT(entry, _t, _l)                                                                  \
    (entry)->type = _t;                                                                            \
    (entry)->length = _l;                                                                          \
    (entry)->cpu = cpuid_get();

static void _switch_task(void *from, void *to)
{
    T_TTOS_TaskControlBlock *old = from;
    T_TTOS_TaskControlBlock *new = to;
    struct trace_switch_task_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_SWITCH_THREAD, sizeof(struct trace_switch_task_entry));

    entry.timestamp = ktime_get_ns();
    entry.prev_tid = old->tid;
    entry.next_tid = new->tid;
    entry.prev_state = old->state;
    entry.prev_prio = corePriorityToPthread(old->taskCurPriority);
    entry.next_prio = corePriorityToPthread(new->taskCurPriority);

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _task_bind_cpu(void *_task)
{
    struct trace_task_bind_cpu_entry entry;
    T_TTOS_TaskControlBlock *task = _task;

    entry.type = TRACE_TYPE_TASK_BIND_CPU;
    entry.length = sizeof(struct trace_task_bind_cpu_entry);
    entry.bind_cpu = task->smpInfo.affinityCpuIndex & 0xFF;
    entry.tid = task->tid;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _waking_task(void *_task)
{
    struct trace_waking_task_entry entry;
    T_TTOS_TaskControlBlock *task = _task;

    ENTRY_INIT(&entry, TRACE_TYPE_SCHED_WAKING, sizeof(struct trace_waking_task_entry));

    entry.prio = task->taskCurPriority;
    entry.tid = task->tid;
    entry.timestamp = ktime_get_ns();

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _irq_handle(uint32_t virq, uint64_t stimestamp, uint32_t duration)
{
    struct trace_irq_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_IRQ, sizeof(struct trace_irq_entry));

    entry.irq = virq;
    entry.duration = duration;
    entry.timestamp = stimestamp;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _syscall(uint32_t id, uint32_t is_entry, void *data)
{
    char buf[256];
    int length;
    struct trace_syscall_entry *entry = (void *)buf;

    length = is_entry ? syscall_get_argc_count(id) : 1;

    entry->type = TRACE_TYPE_SYSCALL;
    entry->cpu = cpuid_get();
    entry->argc = length;
    entry->id = id;
    entry->sts = ((sizeof(long) == 8) ? BIT(1) : 0) | (is_entry ? BIT(0) : 0);
    entry->timestamp = ktime_get_ns();
    length *= sizeof(long);
    entry->length = sizeof(struct trace_syscall_entry) + length;

    memcpy(entry->args, data, length);
    mpscq_enqueue_bytes(&wrok_trace.mpscq, entry, entry->length);
}

void _signal_generate(int32_t code, uint32_t pgid, uint32_t pid, int32_t sig, int32_t result)
{
    struct trace_sig_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_SIG, sizeof(struct trace_sig_entry));

    entry.sig = sig;
    entry.code = code;
    entry.pgid = pgid;
    entry.pid = pid;
    entry.sa_flags_ret = result;
    entry.timestamp = ktime_get_ns();

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

void _signal_deliver(int32_t code, int32_t sig, int32_t sa_flags)
{
    struct trace_sig_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_SIG, sizeof(struct trace_sig_entry) - 8);

    entry.sig = sig;
    entry.code = code;
    entry.sa_flags_ret = sa_flags;
    entry.timestamp = ktime_get_ns();

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _mark(const void *name_point, uint32_t begin)
{
    struct trace_mark_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_MARK, sizeof(struct trace_mark_entry));

    entry.flag = (sizeof(long) == 8) ? BIT(1) : 0;
    entry.flag |= begin;
    entry.timestamp = ktime_get_ns();
    entry.func = (char *)name_point;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _malloc(uint32_t alloc_size, uint32_t req_size, uint32_t flag, void *call_site,
                    void *ptr)
{
    struct trace_malloc_free_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_MALLOC_FREE, sizeof(struct trace_malloc_free_entry));

    entry.flag = (sizeof(long) == 8) ? BIT(1) : 0;
    entry.mflag = flag;
    entry.timestamp = ktime_get_ns();
    entry.call_site = call_site;
    entry.ptr = ptr;
    entry.alloc_size = alloc_size;
    entry.req_size = req_size;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _free(void *call_site, void *ptr)
{
    struct trace_malloc_free_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_MALLOC_FREE, sizeof(struct trace_malloc_free_entry) - 8);

    entry.flag = (sizeof(long) == 8) ? BIT(1) : 0;
    entry.timestamp = ktime_get_ns();
    entry.call_site = call_site;
    entry.ptr = ptr;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _exec(char *filename)
{
    char buf[256];
    struct trace_process_sts_entry *entry = (void *)buf;
    char *p = (char *)&entry->filename;

    entry->type = TRACE_TYPE_PROCESS_STS;
    entry->cpu = cpuid_get();
    entry->ps_type = 1;
    entry->timestamp = ktime_get_ns();

    p = stpcpy(p, filename) + 1;
    entry->length = p - buf;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, entry, entry->length);
}

static void _fork(void *child_task)
{
    T_TTOS_TaskControlBlock *child = child_task;

    struct trace_process_sts_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_PROCESS_STS,
               sizeof(struct trace_process_sts_entry) - sizeof(void *));

    entry.timestamp = ktime_get_ns();
    entry.ps_type = 2;
    entry.args_pid = child->tid;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _exit(void)
{
    struct trace_process_sts_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_PROCESS_STS,
               sizeof(struct trace_process_sts_entry) - sizeof(void *));

    entry.timestamp = ktime_get_ns();
    entry.ps_type = 3;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _compete(void *call_site, uint32_t type, uint32_t duration)
{
    struct trace_compete_entry entry;

    ENTRY_INIT(&entry, TRACE_TYPE_COMPETE, sizeof(struct trace_compete_entry));

    entry.timestamp = ktime_get_ns();
    entry.compete_type = type;
    entry.duration = duration;
    entry.call_site = call_site;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, &entry, entry.length);
}

static void _log(uint32_t level, char *tag, char *msg, uint32_t len)
{
    struct trace_log_entry *entry;
    mpscq_token_t tok;
    char *p;

    len += sizeof(struct trace_log_entry) + 2;
    entry = mpscq_alloc_bytes(&wrok_trace.mpscq, len, &tok);
    if (!entry)
        return;

    ENTRY_INIT(entry, TRACE_TYPE_LOG, len);

    p = (char *)&entry->tag;
    entry->type = TRACE_TYPE_LOG;
    entry->length = len;
    entry->level = level;
    entry->timestamp = ktime_get_ns();
    p = stpcpy(p, tag) + 1;
    stpcpy(p, msg);
    (void) mpscq_push_bytes(&wrok_trace.mpscq, tok, len);
}

static void _object_def(uint32_t type, void *data)
{
    int len;
    char buf[256];
    struct trace_object_entry *obj = (void *)buf;
    char *p = (char *)&obj->name;

    obj->type = TRACE_TYPE_OBJECT;
    obj->objtype = type;

    switch (type)
    {
    case TRACE_OBJ_KTHREAD:
    {
        T_TTOS_TaskControlBlock *task = data;
        obj->uuid = KTHREAD_UUID(task->tid);
        obj->bind_cpu = task->smpInfo.affinityCpuIndex;
        p = stpcpy(p, task->objCore.objName) + 1;
        obj->length = p - buf;
        // printk("Kthread obj uuid:0x%08x [%d : %d]:  %s\n", obj->uuid,
        //         task->ppcb ? get_process_pid((pcb_t) task->ppcb) : 0,
        //         task->tid, task->objCore.objName);
        // printk("Kthread obj tid[%d]:  %s\n", task->tid, task->objCore.objName);
        break;
    }

    case TRACE_OBJ_THREAD:
    {
        T_TTOS_TaskControlBlock *task = data;
        pcb_t pcb = task->ppcb;

        obj->objtype = TRACE_OBJ_THREAD;
        obj->uuid = THREAD_UUID(pcb->tgid, task->tid);
        obj->bind_cpu = task->smpInfo.affinityCpuIndex;
        p = stpcpy(p, task->objCore.objName) + 1;
        obj->length = p - buf;
        // printk("thread obj uuid:0x%08x [%d : %d : %d]: %s\n", obj->uuid,
        //         get_process_pid(pcb), pcb->tgid, task->tid, task->objCore.objName);
        // printk("Thread obj tid[%d]:  %s\n", task->tid, task->objCore.objName);
        break;
    }

    case TRACE_OBJ_PROCESS:
    {
        T_TTOS_TaskControlBlock *task = data;
        pcb_t pcb = task->ppcb;

        obj->bind_cpu = task->smpInfo.affinityCpuIndex;
        if (!pcb->group_leader->parent)
            obj->uuid = PROESS_UUID(get_process_pid(pcb), get_process_pid(pcb));
        else
            obj->uuid =
                PROESS_UUID(get_process_pid(pcb->group_leader->parent), get_process_pid(pcb));
        p = stpncpy(p, task->objCore.objName, 64) + 1;
        p[-1] = 0;
        p = stpncpy(p, ((pcb_t)task->ppcb)->cmdline->really->obj, 64) + 1;
        p[-1] = 0;
        obj->length = p - buf;
        // printk("process obj pcb(%p) uuid:0x%08x [%d : %d]: %s\n", pcb, obj->uuid,
        //         pcb->group_leader->parent ? get_process_pid(pcb->group_leader->parent) : 0,
        //         get_process_pid(pcb), task->objCore.objName);
        // printk("Process obj tid[%d]:  %s\n", task->tid, task->objCore.objName);
        break;
    }

    case TRACE_OBJ_IRQ:
    {
        ttos_irq_desc_t desc = data;
        p = stpcpy(p, desc->name) + 1;
        obj->irq = desc->virt_irq;
        obj->length = p - buf;
        // printk("desc: %s  irq: %d\n", desc->name, desc->virt_irq);
    }
    break;

    default:
        return;
    }

    mpscq_enqueue_bytes(&wrok_trace.mpscq, obj, obj->length);
}

static void trae_system_info(void)
{
    static char machine[] = {
#if defined(CONFIG_ARCH_ARMv7)
        "armv7l"
#elif defined(CONFIG_ARCH_AARCH32)
        "armv8l"
#elif defined(CONFIG_ARCH_AARCH64)
        "aarch64"
#elif defined(CONFIG_ARCH_X86_64)
        "x86_64"
#endif
    };

    char buf[256];
    char *p;
    struct trace_system_info_entry *entry;

    entry = (struct trace_system_info_entry *)buf;
    p = (char *)&entry->sysname;
    entry->type = TRACE_TYPE_SYSTEM_INFO;
    entry->timestamp =  ktime_get_ns();
    entry->clock_freq = ttos_time_freq_get();
    p = stpcpy(p, "InteWell") + 1;
    p = stpcpy(p, INTEWELL_VERSION_STRING) + 1;
    p = stpcpy(p, machine) + 1;
    p = stpcpy(p, GIT_INFO) + 1;

    entry->length = p - buf;

    mpscq_enqueue_bytes(&wrok_trace.mpscq, entry, entry->length);
}

static void add_task_obj(TASK_ID task, void *par)
{
    if (task->ppcb)
    {
        if (task->tid > CONFIG_PROCESS_MAX)
            _object_def(TRACE_OBJ_THREAD, task);
        else
            _object_def(TRACE_OBJ_PROCESS, task);
    }
    else
    {
        _object_def(TRACE_OBJ_KTHREAD, task);
    }
}

static void add_irq_obj(ttos_irq_desc_t desc, void *par)
{
    if (desc->name[0])
        _object_def(TRACE_OBJ_IRQ, desc);
}

static inline void _file_flush_cache(struct trace_user *trace)
{
    if (trace->cache_offset > 0) {
        file_write(trace->f, trace->data_cache, trace->cache_offset);
        trace->cache_offset = 0;
    }
}

static void _file_write(struct trace_user *trace, const void *buf, size_t nbytes)
{
    if (!trace->f)
        return;

    // 约定：buf==NULL 或 nbytes==0 => 触发 flush（用于收尾）
    if (!buf || nbytes == 0) {
        _file_flush_cache(trace);
        return;
    }

    const uint8_t *p = (const uint8_t *) buf;
    size_t left = nbytes;

    // 情况1：这次写入本身 >= cache_sz
    // 策略：先 flush 当前 cache，再直接写入（可分段写，避免一次太大）
    if (left >= trace->cache_sz) {
        _file_flush_cache(trace);

        // 分段直写：每次最多 cache_sz
        while (left > 0) {
            size_t chunk = left;
            if (chunk > trace->cache_sz)
                chunk = trace->cache_sz;
            file_write(trace->f, p, chunk);
            p += chunk;
            left -= chunk;
        }
        return;
    }

    // 情况2：正常聚合写入（left < cache_sz）
    // 若放不下，则先 flush，再拷贝
    if (trace->cache_offset + left > trace->cache_sz) {
        _file_flush_cache(trace);
    }

    memcpy(trace->data_cache + trace->cache_offset, p, left);
    trace->cache_offset += left;

    // 可选：刚好写满则立即刷（减少尾部残留）
    if (trace->cache_offset == trace->cache_sz) {
        _file_flush_cache(trace);
    }
}


static void trace_worker(void *arg)
{
    struct trace_user *trace = arg;
    uint8_t data[512];
    int mq_count;
    int save_count = 0;
    int max_count;
    int ret;
    int signal_max_count = 5000;

    if (trace->slot->used && trace->filesystem_ready && !trace->f)
    {
        unlink("/tmp/trace.bin");
        ret = file_open(&trace->f_stored, "/tmp/trace.bin", O_RDWR | O_CREAT | O_TRUNC);
        if (ret == 0)
        {
            trace->f = &trace->f_stored;
        }
    }

    max_count = mpscq_count(&trace->mpscq);

    if (trace->slot->used)
    {
        max_count = max_count < signal_max_count ? signal_max_count : max_count >> 2;
    }

    while (trace->f && (save_count < max_count))
    {
        mq_count = mpscq_dequeue(&trace->mpscq, data, 1);
        if (mq_count == -1)
            break;

        mq_count = TRACE_MQ_NUM(data[1]) - 1;

        if (mq_count)
            mpscq_dequeue(&trace->mpscq, data + TRACE_ELEM_SIZE, mq_count);

        _file_write(trace, data, data[1]);
        save_count++;
    }
    _file_write(trace, NULL, 0);

    if (!trace->slot->used)
    {
        if (save_count == 0)
        {
            mpscq_destroy(&trace->mpscq);
            if (trace->f)
            {
                file_close(trace->f);
                trace->f = NULL;
                // printk("Trace worker: all work done, closing trace file.\n");
                return;
            }
        }
        else if (!trace->f)
        {
            /* 有消息, fd未创建, 直接销毁队列 */
            mpscq_destroy(&trace->mpscq);
            // printk("Trace worker: fd not created but messages exist, destroying queue.\n");
            return;
        }
    }

    work_queue(&trace->work, trace_worker, arg, 5, NULL);
}

static void _config(struct trace_slot *slot)
{
    if (wrok_trace.slot->used)
    {
        if (!wrok_trace.mpscq.data)
        {
            wrok_trace.slot->used = false;
            // wmb();
            mpscq_init(&wrok_trace.mpscq, TRACE_ELEM_NUM, TRACE_ELEM_SIZE);
            wrok_trace.slot->used = true;
            trae_system_info();
        }

        task_foreach(add_task_obj, NULL);
        irqdesc_foreach(add_irq_obj, NULL);

        if (wrok_trace.work_ready)
            work_queue(&wrok_trace.work, trace_worker, &wrok_trace, 1, NULL);
    }
    else
    {
        // printk("Trace disabled, cleaning up resources. mpscq: %d, %d\n",
        //         wrok_trace.mpscq.head, wrok_trace.mpscq.tail);
        if (wrok_trace.work_ready)
            work_queue(&wrok_trace.work, trace_worker, &wrok_trace, 1, NULL);
    }
}

static void trace_init(struct trace_slot *slot)
{
    wrok_trace.slot = slot;

    if (IS_ENABLED(CONFIG_TRACE_BOOT))
    {
        wrok_trace.slot->used = true;
        _config(slot);
    }

    wrok_trace.cache_sz = 8192;
    wrok_trace.cache_offset = 0;
    wrok_trace.data_cache = malloc(wrok_trace.cache_sz);
}

static struct trace_ops ops = {
    .init            = trace_init,
    .switch_task     = _switch_task,
    .waking_task     = _waking_task,
    .irq             = _irq_handle,
    .object          = _object_def,
    .syscall         = _syscall,
    .signal_generate = _signal_generate,
    .signal_deliver  = _signal_deliver,
    .log             = _log,
    .mark            = _mark,
    .task_bindcpu    = _task_bind_cpu,
    .malloc          = _malloc,
    .free            = _free,
    .exec            = _exec,
    .fork            = _fork,
    .exit            = _exit,
    .compete         = _compete,
    .config          = _config,
};

static int trace_setup(void)
{
    return trace_register("perfetto", &ops);
}
INIT_EXPORT_ARCH(trace_setup, "trace_setup");

static int trace_setup2(void)
{
    if (wrok_trace.slot)
    {
        wrok_trace.filesystem_ready = true;
        wrok_trace.work_ready = true;
        work_queue(&wrok_trace.work, trace_worker, &wrok_trace, 1, NULL);
    }
}
INIT_EXPORT_SERVE_FS(trace_setup2, "trace_setup2");
