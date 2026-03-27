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
 * @file arch_signal.c
 * @brief AArch64 信号帧构造与恢复实现。
 */

#include <assert.h>
#include <cache.h>
#include <context.h>
#include <errno.h>
#include <process_signal.h>
#include <sigcontext.h>
#include <signal.h>
#include <syscall.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <util.h>
#include <system/bitops.h>

#undef KLOG_TAG
#define KLOG_TAG "arch_signal"
#include <klog.h>

/* ==================== 常量与类型 ==================== */

#define SZ_256K        0x00040000
#define SIGFRAME_MAXSZ SZ_256K

#define AARCH64_SIGFRAME_ALIGN     16UL
#define AARCH64_INSN_SIZE          4UL
#define AARCH64_THUMB_INSN_SIZE    2UL
#define SIGFRAME_RESERVED_OFFSET \
    offsetof(struct signal_frame, uc.uc_mcontext.__reserved)
#define SIGFRAME_RESERVED_SIZE \
    sizeof(((struct signal_frame *)0)->uc.uc_mcontext.__reserved)

/*
    |                 __reserved[] 总空间                           |
    [ 普通记录可用区 | EXTRA_CONTEXT_SIZE 预留 | END_CONTEXT_SIZE 预留 ]
*/
#define END_CONTEXT_SIZE    round_up(sizeof(struct _aarch64_ctx), 16)
#define EXTRA_CONTEXT_SIZE  round_up(sizeof(struct extra_context), 16)

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a)-1)) == 0)

#define GENMASK_ULL(h, l)                                                      \
    (((~0ULL) << (l)) & (~0ULL >> (sizeof(long long) * 8 - 1 - (h))))

#ifdef CONFIG_COMPAT
#define compat_thumb_mode(regs) ((regs)->pstate & PSR_AA32_T_BIT)
#else
#define compat_thumb_mode(regs) (0)
#endif

#define AARCH64_USER_CPSR_EL0T_MODE         0x00000000
#define AARCH64_USER_CPSR_MODE_MASK         0x0000000f
#define AARCH64_USER_CPSR_AARCH32_STATE_BIT 0x00000010
#define AARCH64_USER_CPSR_FIQ_MASK_BIT      0x00000040
#define AARCH64_USER_CPSR_IRQ_MASK_BIT      0x00000080
#define AARCH64_USER_CPSR_SERROR_MASK_BIT   0x00000100
#define AARCH64_USER_CPSR_DEBUG_MASK_BIT    0x00000200
#define AARCH64_USER_CPSR_FLAGS_MASK        0xf0000000
#define AARCH64_USER_CPSR_BTYPE_MASK        0x00000c00
#define AARCH64_USER_CPSR_TCO_BIT           0x02000000
#define AARCH64_USER_CPSR_BTYPE_SHIFT       10
#define AARCH64_USER_CPSR_BTYPE_C \
    (0b10 << AARCH64_USER_CPSR_BTYPE_SHIFT)

/*
 * uc.uc_mcontext.__reserved[] 保存一串变长上下文记录。每条记录都以
 * struct _aarch64_ctx 为头部，解析时依次读取 head.magic / head.size。
 *
 * __reserved[] 内联布局示意：
 * +---------------------------+
 * | context1                  |
 * |   head                    |
 * |   payload                 |
 * +---------------------------+
 * | context2                  |
 * |   head                    |
 * |   payload                 |
 * +---------------------------+
 * | ...                       |
 * +---------------------------+
 * | extra_context             |
 * |   head                    |
 * |   datap -> 指向外部区域   |
 * |   size  -> 外部区域大小   |
 * +---------------------------+
 * | terminator                |
 * |   head.magic = 0          |
 * |   head.size  = 0          |
 * +---------------------------+
 *
 * 如果 __reserved[] 放不下，就插入 extra_context，告诉内核后面还有一段
 * 帧外扩展区：
 *
 * __reserved[]
 * +---------------------------+
 * | fpsimd_context            |
 * +---------------------------+
 * | extra_context             |
 * |   head                    |
 * |   datap -> 指向外部区域   |
 * |   size  -> 外部区域大小   |
 * +---------------------------+
 * | terminator                |
 * +---------------------------+
 *
 * 帧外扩展区
 * +---------------------------+
 * | sve_context / za / ...    |
 * +---------------------------+
 * | terminator                |
 * +---------------------------+
 */

/**
 * struct signal_frame - AArch64 实时信号帧布局
 *
 * 按照 AArch64 ABI 放置于用户栈，包含 siginfo、ucontext 以及
 * 可选的 restorer trampoline 指令（未设置 SA_RESTORER 时使用）。
 */
struct signal_frame
{
    struct siginfo info;
    ucontext_t     uc;

    /* 内置 trampoline：mov x8, #__NR_rt_sigreturn + svc #0（共 8 字节）*/
    unsigned int sig_return_code[2];
};

/* 为调试器回溯使用，与信号本身功能无关 */
struct sigframe_unwind_record
{
    uint64_t fp;
    uint64_t lr;
};

/**
 * struct signal_frame_layout - 信号帧各扩展记录的偏移量索引
 *
 * AArch64 信号帧在 uc_mcontext.__reserved[] 区域追加可选上下文记录
 * （FPSIMD、SVE、SME 等），本结构记录各记录相对 signal_frame 起始的偏移。
 */
struct signal_frame_layout
{
    struct signal_frame   *sigframe;
    struct sigframe_unwind_record *unwind_record;

    /*
     * 分配前，它表示“当前记录的起始 offset”；
     * 分配完成后，它推进为“下一个可分配位置的 offset”。
     */
    unsigned long next_offset;

    /* 当前允许分配的上界 offset（相对 signal_frame 起始） */
    unsigned long offset_max;

    unsigned long fpsimd_offset;
    unsigned long extra_offset;
    unsigned long end_offset;
};

/** struct signal_frame_user_contexts - sigreturn 时从用户态信号帧解析出的扩展上下文视图 */
struct signal_frame_user_contexts
{
    struct fpsimd_context  *fpsimd;
    uint32_t fpsimd_size;
};

/* ==================== 前置声明 ==================== */

extern void sigreturn_code(void);
extern void restore_raw_context(arch_exception_context_t *context);

/* ==================== 当前支持的扩展能力 ==================== */

#define AARCH64_SIGNAL_HAS_FPSIMD 1
#define AARCH64_SIGNAL_HAS_SVE    0
#define AARCH64_SIGNAL_HAS_SME    0
#define AARCH64_SIGNAL_HAS_SME2   0
#define AARCH64_SIGNAL_HAS_TPIDR2 0
#define AARCH64_SIGNAL_HAS_FPMR   0
#define AARCH64_SIGNAL_HAS_BTI    0

#define AARCH64_USER_CPSR_RESERVED_ZERO_MASK                                   \
    (GENMASK_ULL (63, 32) | GENMASK_ULL (27, 26) | GENMASK_ULL (23, 22)       \
     | GENMASK_ULL (20, 13) | GENMASK_ULL (5, 5))

static inline bool aarch64_context_runs_at_el0 (const struct arch_context *context)
{
    return ((context->cpsr & AARCH64_USER_CPSR_MODE_MASK)
            == AARCH64_USER_CPSR_EL0T_MODE);
}

static int aarch64_validate_return_context (struct arch_context *context)
{
    context->cpsr &= ~AARCH64_USER_CPSR_RESERVED_ZERO_MASK;

    if (aarch64_context_runs_at_el0 (context)
        && !(context->cpsr & AARCH64_USER_CPSR_AARCH32_STATE_BIT)
        && !(context->cpsr & AARCH64_USER_CPSR_DEBUG_MASK_BIT)
        && !(context->cpsr & AARCH64_USER_CPSR_SERROR_MASK_BIT)
        && !(context->cpsr & AARCH64_USER_CPSR_IRQ_MASK_BIT)
        && !(context->cpsr & AARCH64_USER_CPSR_FIQ_MASK_BIT))
    {
        return 1;
    }

    context->cpsr &= AARCH64_USER_CPSR_FLAGS_MASK;

    return 0;
}

/* ==================== 信号帧空间分配 ==================== */

/**
 * signal_frame_layout_init - 初始化信号帧布局描述符
 *
 * 初始可分配区域从 signal_frame 固定部分之后开始，也就是
 * uc.uc_mcontext.__reserved[] 的起始位置。该区域用于放置
 * fpsimd_context / sve_context / extra_context 等可选扩展记录。
 *
 * 这里会先从 __reserved[] 总容量中扣除两部分保留空间：
 * 1. 末尾必须存在的零终止记录；
 * 2. 当内联空间不足时，可能需要补充的 extra_context 头部。
 *
 * 这样后续分配逻辑就可以优先在“安全水位”内顺序追加记录，
 * 只有确实需要时才显式启用 extra_context 扩展。
 */
static void signal_frame_layout_init(struct signal_frame_layout *layout)
{
    /*
        |                 __reserved[] 总空间                  |
        [ 普通记录可用区 | extra_context 预留 | terminator 预留 ]
    */
    const size_t reserved_size = SIGFRAME_RESERVED_SIZE;

    /* 记录保留空间的起始偏移 */
    layout->next_offset = SIGFRAME_RESERVED_OFFSET;

    /* 记录保留空间的最大偏移 */
    layout->offset_max = layout->next_offset + reserved_size;

    /*
     * 留给末尾的终止记录 struct _aarch64_ctx { magic=0, size=0 }。
     * 解析 __reserved[] 时要靠它判断“记录到这里结束了”，所以这块空间必须保留
     */
    layout->offset_max -= END_CONTEXT_SIZE;

    /*
     * 留给可选的 extra_context 头。
     * 如果后面发现 __reserved[] 不够放更多扩展记录，就可以在这里塞一个
     * extra_context，告诉解析代码“真正的扩展数据在帧外扩展区”。
     */
    layout->offset_max -= EXTRA_CONTEXT_SIZE;
}

/**
 * signal_frame_layout_size - 计算最终需要拷贝到用户栈的信号帧大小
 *
 * layout->next_offset 记录的是当前已经规划出来的实际布局末尾偏移；而
 * struct signal_frame 则代表最小固定信号帧尺寸。两者取较大值后
 * 再按 AArch64 ABI 要求对齐到 16 字节。
 */
static size_t signal_frame_layout_size(const struct signal_frame_layout *layout)
{
    return round_up(max(layout->next_offset, sizeof(struct signal_frame)),
                    AARCH64_SIGFRAME_ALIGN);
}

/**
 * signal_frame_layout_reserve - 在 __reserved[] 中分配 size 字节的扩展记录
 * @offset: 返回新记录在整个 signal_frame 中的起始偏移
 * @size:   记录原始大小，函数内部会按 16 字节对齐
 * @extend: 是否允许在空间不足时引入 extra_context 进行扩展
 *
 * 分配成功时将偏移量写入 *offset 并返回 0；空间不足返回 -ENOMEM。
 *
 * 分配策略分两步：
 * 1. 先尝试直接在 __reserved[] 当前剩余空间中顺序追加；
 * 2. 若空间不足且允许扩展，并且此前尚未放入 extra_context，
 *    则先为 extra_context 自身分配槽位，再把 offset_max 放宽到
 *    SIGFRAME_MAXSZ，对应 Linux/AArch64 信号帧的扩展模式。
 */
static int signal_frame_layout_reserve(struct signal_frame_layout *layout,
                                       unsigned long *offset,
                                       size_t size,
                                       bool extend)
{
    size_t padded_size = round_up(size, 16);
    int ret;

    if (padded_size > layout->offset_max - layout->next_offset &&
        !layout->extra_offset && extend)
    {
        /*
         * 先把初始化阶段预留出来的 extra_context 头部空间“取回来”，
         * 然后递归为 extra_context 记录本身分配一个槽位。
         */
        layout->offset_max += EXTRA_CONTEXT_SIZE;
        ret = signal_frame_layout_reserve(layout, &layout->extra_offset,
                                          sizeof(struct extra_context), false);
        if (ret)
        {
            layout->offset_max -= EXTRA_CONTEXT_SIZE;
            return ret;
        }

        /* 为终止记录预留空间 */
        layout->next_offset += END_CONTEXT_SIZE;

        /*
         * 开启扩展模式后，后续记录可以继续增长到 SIGFRAME_MAXSZ，
         * 但末尾仍需保留一个终止记录位置，因此这里继续扣掉
         * END_CONTEXT_SIZE。
         */
        layout->offset_max = SIGFRAME_MAXSZ - END_CONTEXT_SIZE;
    }

    if (padded_size > layout->offset_max - layout->next_offset)
    {
        return -ENOMEM;
    }

    /* 分配前 next_offset 是当前记录起始 offset，写回后再推进到下一个槽位。 */
    *offset = layout->next_offset;
    layout->next_offset += padded_size;

    return 0;
}

/* 默认允许在内联空间不足时自动切换到 extra_context 扩展模式。 */
static int signal_frame_layout_alloc(struct signal_frame_layout *layout,
                                     unsigned long *offset,
                                     size_t size)
{
    return signal_frame_layout_reserve(layout, offset, size, true);
}

/**
 * signal_frame_layout_plan - 确定各扩展上下文记录在信号帧中的布局
 * @layout: 输出：各扩展记录在 signal_frame 中的偏移信息
 */
static int signal_frame_layout_plan(struct signal_frame_layout *layout)
{
    int ret;

    /* 目前仅支持 fpsimd_context，其余扩展上下文暂未实现 */
    if (AARCH64_SIGNAL_HAS_FPSIMD)
    {
        /* 在保留空间中分配空间,并记录空间的offset到layout->fpsimd_offset中 */
        ret = signal_frame_layout_alloc(layout, &layout->fpsimd_offset,
                                        sizeof(struct fpsimd_context));
        if (ret)
        {
            return ret;
        }
    }

        /* 释放为终止记录预留的空间，然后正式分配 */
    layout->offset_max += END_CONTEXT_SIZE;

    ret = signal_frame_layout_alloc(layout, &layout->end_offset,
                                    sizeof(struct _aarch64_ctx));
    if (ret)
        return ret;

    /* 分配结束, 禁止后续分配 */
    layout->offset_max = layout->next_offset;

    return 0;
}

static void *signal_frame_record_ptr(const struct signal_frame_layout *layout,
                                     unsigned long offset)
{
    return (char *)layout->sigframe + offset;
}

/* ==================== 信号帧布局 ==================== */

/*
 * signal_frame_prepare() 在用户栈上的布局如下（栈从高地址向低地址增长）：
 *
 *   high address
 *        ^
 *        |
 *        |  sp_top = signal_sp_get(context->sp, ksig)
 *        |  +--------------------------------------+
 *        |  | possible alignment gap              |
 *        |  +--------------------------------------+
 *        |  | struct sigframe_unwind_record       |
 *        |  |   fp                                |
 *        |  |   lr                                |
 *        |  +--------------------------------------+ <- layout->unwind_record
 *        |  | struct signal_frame                 |
 *        |  |   siginfo                           |
 *        |  |   ucontext                          |
 *        |  |     uc_mcontext.__reserved[]        |
 *        |  |       fpsimd_context / end record   |
 *        |  |   sig_return_code[2]                |
 *        |  +--------------------------------------+ <- layout->sigframe
 *        |
 *        v
 *   low address
 */

/**
 * signal_frame_prepare - 在用户栈上分配并初始化信号帧
 * @layout: 输出：各记录的偏移量索引及信号帧指针
 * @ksig: 待投递的信号（含备用栈信息）
 * @context: 被中断时的寄存器上下文
 *
 * 返回 0 表示成功；返回负值表示用户栈不可写或空间不足。
 */
static int signal_frame_prepare(struct signal_frame_layout *layout,
                                struct ksignal *ksig,
                                struct arch_context *context)
{
    unsigned long sp;
    unsigned long sp_top;
    int ret;

    /* 初始化保留空间的起始偏移，最大偏移，并保留结束空间头和额外空间上下文空间 */
    signal_frame_layout_init(layout);

    /* 在保留空间中，分配浮点上下文空间 */
    ret = signal_frame_layout_plan(layout);
    if (ret)
    {
        return ret;
    }

    sp = sp_top = signal_sp_get(context->sp, ksig);

    /* 在信号帧顶部为栈展开记录（fp/lr）留出空间。 */
    sp = round_down(sp - sizeof(struct sigframe_unwind_record),
                    AARCH64_SIGFRAME_ALIGN);

    layout->unwind_record = (struct sigframe_unwind_record *)sp;

    sp = round_down(sp, AARCH64_SIGFRAME_ALIGN) - signal_frame_layout_size(layout);
    layout->sigframe = (struct signal_frame *)sp;

    if (!user_access_check(layout->sigframe, sp_top - sp, UACCESS_W))
        return -EFAULT;

    memset(layout->sigframe, 0, sizeof(*layout->sigframe));

    return 0;
}

/* ==================== 信号上下文构建 ==================== */

static void signal_frame_save_mcontext(struct signal_frame *sigframe,
                                       const struct arch_context *context,
                                       const k_sigset_t *set)
{
    memcpy(&sigframe->uc.uc_mcontext.regs, &context->regs,
           sizeof(sigframe->uc.uc_mcontext.regs));
    sigframe->uc.uc_mcontext.sp            = context->sp;
    sigframe->uc.uc_mcontext.pc            = context->elr;
    sigframe->uc.uc_mcontext.pstate        = context->cpsr;
    sigframe->uc.uc_mcontext.fault_address = 0;

    memcpy(&sigframe->uc.uc_sigmask, set, sizeof(*set));
}

static int save_fpsimd_context_to_sigframe(struct fpsimd_context  *ctx,
                                           struct arch_context *context)
{
    if (copy_to_user(ctx->vregs, &context->fpContext.vRegs, sizeof(ctx->vregs)))
    {
        KLOG_EMERG("build_signal_frame: copy siginfo to user failed");
        return -EFAULT;
    }

    ctx->fpsr = context->fpContext.fpsr;
    ctx->fpcr = context->fpContext.fpcr;

    ctx->head.magic = FPSIMD_MAGIC;
    ctx->head.size  = sizeof(struct fpsimd_context);

    return 0;
}

static void save_end_context_to_sigframe(struct _aarch64_ctx *ctx)
{
    ctx->magic = 0;
    ctx->size  = 0;
}

static void save_extra_context_to_sigframe(const struct signal_frame_layout *layout)
{
    char *sigframe_base;
    char *userp;
    char *extra_base;
    struct extra_context *extra;
    struct _aarch64_ctx *end;
    size_t frame_size;

    if (!layout->extra_offset)
        return;

    sigframe_base = (char *)layout->sigframe;
    userp = signal_frame_record_ptr(layout, layout->extra_offset);

    extra = (struct extra_context *)userp;
    userp += EXTRA_CONTEXT_SIZE;

    /*
     * extra_context 在 __reserved[] 中必须紧跟一个零终止记录。
     * 真正的扩展数据区从该终止记录之后开始，最终仍以一个零终止记录结束。
     */
    end = (struct _aarch64_ctx *)userp;
    userp += END_CONTEXT_SIZE;
    extra_base = userp;

    frame_size = signal_frame_layout_size(layout);

    extra->head.magic = EXTRA_MAGIC;
    extra->head.size  = EXTRA_CONTEXT_SIZE;
    extra->datap      = (u64)(unsigned long)extra_base;
    extra->size       = (uint32_t)((sigframe_base + frame_size) - extra_base);

    save_end_context_to_sigframe(end);
}

static int signal_frame_copy_siginfo(struct signal_frame *sigframe,
                                     const struct ksignal *ksig,
                                     struct arch_context *context)
{
    if (copy_to_user(&sigframe->info, &ksig->info, sizeof(sigframe->info)))
    {
        KLOG_EMERG("build_signal_frame: copy siginfo to user failed");
        return -EFAULT;
    }

    context->regs[1] = (unsigned long)&sigframe->info;
    context->regs[2] = (unsigned long)&sigframe->uc;

    return 0;
}

/**
 * signal_frame_populate - 将寄存器上下文和信号屏蔽字写入信号帧
 * @layout: 信号帧布局索引
 * @context: 被中断时的内核寄存器上下文
 * @set:  当前信号屏蔽字
 */
static int signal_frame_populate(struct signal_frame_layout *layout,
                                 struct arch_context *context,
                                 const k_sigset_t *set)
{
    struct signal_frame *sigframe = layout->sigframe;
    struct _aarch64_ctx *end_ctx;

    /* 填写用于栈展开的 fp/lr 链记录。 */
    layout->unwind_record->fp = context->regs[29];
    layout->unwind_record->lr = context->regs[30];

    signal_frame_save_mcontext(sigframe, context, set);

    /* FPSIMD 扩展上下文 */
    if (AARCH64_SIGNAL_HAS_FPSIMD)
    {
        /* fpsimd_offset 在布局规划阶段生成，这里按该偏移定位到记录槽位。 */
        struct fpsimd_context *fpsimd_ctx;
        int ret;

        fpsimd_ctx = signal_frame_record_ptr(layout, layout->fpsimd_offset);
        ret = save_fpsimd_context_to_sigframe(fpsimd_ctx, context);
        if (ret)
            return ret;
    }

    save_extra_context_to_sigframe(layout);

    /*
     * 显式写入结束记录，而不是仅依赖 prepare() 阶段对 signal_frame
     * 固定部分的清零副作用。解析 __reserved[] 时以 magic=0,size=0
     * 作为终止条件。
     */
    end_ctx = signal_frame_record_ptr(layout, layout->end_offset);
    save_end_context_to_sigframe(end_ctx);

    return 0;
}

/**
 * signal_frame_setup_handler - 配置 handler 入口的寄存器状态
 * @regs: 被中断时的寄存器上下文（将被修改为 handler 入口）
 * @ksig:  待投递的信号
 * @layout: 信号帧布局索引
 * @usig:  传给 handler 的第一个参数（signo）
 *
 * 未设置 SA_RESTORER 时，将内置 trampoline 代码写入信号帧并刷新 I-Cache。
 */
static void signal_frame_setup_handler(struct arch_context *context,
                                       struct ksignal *ksig,
                                       struct signal_frame_layout *layout,
                                       int usig)
{
    unsigned long sigtramp;

    context->regs[0]  = usig;
    context->sp       = (unsigned long)layout->sigframe;
    context->regs[29] = (unsigned long)&layout->unwind_record->fp;
    context->elr      = (unsigned long)ksig->ka.__sa_handler._sa_handler;

    /*
     * BTI（Branch Target Identification）：信号投递等同于间接跳转（BLR），
     * 将 BTYPE 设置为 C（call），使目标页中的非函数入口点触发 SIGILL。
     */
    if (AARCH64_SIGNAL_HAS_BTI)
    {
        context->cpsr &= ~AARCH64_USER_CPSR_BTYPE_MASK;
        context->cpsr |= AARCH64_USER_CPSR_BTYPE_C;
    }

    /* TCO（Tag Check Override）在 handler 入口处始终清除 */
    context->cpsr &= ~AARCH64_USER_CPSR_TCO_BIT;

    if (ksig->ka.sa_flags & SA_RESTORER)
    {
        sigtramp = (unsigned long)ksig->ka.sa_restorer;
    }
    else
    {
        /* 将内置 trampoline（mov x8, #NR_rt_sigreturn + svc）写入信号帧 */
        memcpy(&layout->sigframe->sig_return_code, sigreturn_code,
               sizeof(layout->sigframe->sig_return_code));
        sigtramp = (unsigned long)&layout->sigframe->sig_return_code;
        cache_text_update(sigtramp, sizeof(layout->sigframe->sig_return_code));
    }

    context->regs[30] = sigtramp;
}

/**
 * build_signal_frame - 构建完整的 signal_frame 并切换到信号处理函数上下文
 * @ksig: 待投递的信号
 * @set:  当前信号屏蔽字
 * @context: 被中断时的寄存器上下文
 *
 * 返回 0 表示成功；非零表示无法构建信号帧。
 */
static int build_signal_frame(struct ksignal *ksig, k_sigset_t *set,
                              struct arch_context *context)
{
    struct signal_frame_layout layout = {0};
    struct signal_frame *frame;
    int ret;

    ret = signal_frame_prepare(&layout, ksig, context);
    if (ret)
    {
        return ret;
    }

    /* frame指向c库定义的信号上下文 */
    frame = layout.sigframe;

    frame->uc.uc_flags = 0;

    frame->uc.uc_link  = NULL;

    altstack_save_to_user(&frame->uc.uc_stack, context->sp);

    ret = signal_frame_populate(&layout, context, set);
    if (ret)
    {
        return ret;
    }

    signal_frame_setup_handler(context, ksig, &layout, ksig->sig);

    if (ksig->ka.sa_flags & SA_SIGINFO)
        return signal_frame_copy_siginfo(frame, ksig, context);

    return 0;
}

/* ==================== 信号投递 ==================== */

/**
 * enter_signal_handler - 完成信号投递：构建信号帧并切换到 handler
 *
 * 该函数不会返回（最终调用 restore_raw_context 切换到用户态）。
 */
static void enter_signal_handler(struct ksignal *ksig, struct arch_context *context)
{
    k_sigset_t *oldset = signal_mask_save_target();

    if (build_signal_frame(ksig, oldset, context))
    {
        TTOS_SuspendTask(TTOS_SELF_OBJECT_ID);
    }

    aarch64_validate_return_context(context);

    signal_delivered(ksig, 0);

    TTOS_TaskEnterUserHook(ttosProcessSelf()->taskControlId);

    /*
     * restore_raw_context(context) 返回用户态前，寄存器/栈关系如下：
     *
     *   当前将要装载到硬件寄存器的值
     *
     *     context->sp       --> layout->sigframe
     *     context->regs[29] --> &layout->unwind_record->fp
     *     context->regs[30] --> sigtramp
     *                           (sa_restorer 或 sigframe->sig_return_code)
     *     context->elr      --> 用户态 signal handler 入口
     *
     *   用户栈中的内容（高地址 -> 低地址）：
     *
     *     +--------------------------------------+ <- 原始 sp_top
     *     | alignment gap / 未使用空间           |
     *     +--------------------------------------+
     *     | struct sigframe_unwind_record        |
     *     |   fp = 原始 x29(fp)                  |
     *     |   lr = 原始 x30(lr)                  |
     *     +--------------------------------------+
     *     | struct signal_frame                  |
     *     |   uc.uc_mcontext.sp = 原始 sp        |
     *     |   uc.uc_mcontext.pc = 原始 elr       |
     *     |   uc.uc_mcontext.regs[] = 原始 x0-x30|
     *     |   ...                                |
     *     |   sig_return_code[2]                 |
     *     +--------------------------------------+ <- 当前 context->sp
     *
     *   因此进入 handler 后：
     *   1) sp  指向当前 signal_frame；
     *   2) fp  指向 unwind_record->fp，从而把原始 fp/lr 串成一条可展开链；
     *   3) lr  指向信号返回 trampoline，handler return 后会跳到这里；
     *   4) elr 指向真正的用户 handler 入口；
     *   5) 原始 sp / elr 不再直接放在寄存器里，而是保存在 signal_frame 中，
     *      后续 rt_sigreturn 再从该帧恢复。
     */
    restore_raw_context(context);
}

/* ==================== 系统调用重启 ==================== */

static unsigned long syscall_restart_insn_size(struct arch_context *context)
{
    return compat_thumb_mode(context) ? AARCH64_THUMB_INSN_SIZE
                                      : AARCH64_INSN_SIZE;
}

static void syscall_restart_addrs(struct arch_context *context,
                                  unsigned long *continue_addr,
                                  unsigned long *restart_addr)
{
    *continue_addr = context->elr;
    *restart_addr  = *continue_addr - syscall_restart_insn_size(context);
}

/**
 * do_syscall_restart_check - 在信号投递前调整系统调用返回值（可选路径）
 */
void do_syscall_restart_check(struct arch_context *context, struct ksignal *ksignal)
{
    unsigned long continue_addr;
    unsigned long restart_addr;
    int retval;

    if (SYSCALL_CONTEXT != context->type)
        return;

    syscall_restart_addrs(context, &continue_addr, &restart_addr);
    retval = context->regs[0];

    if (-ERR_RESTART_IF_NO_HANDLER == retval)
    {
        context->regs[0] = -EINTR;
        context->elr     = continue_addr;
    }
    else if (ksignal && -EINTR == retval && (ksignal->ka.sa_flags & SA_RESTART))
    {
        context->regs[0] = context->ori_x0;
        context->elr     = restart_addr;
    }
}

static void install_restart_syscall(struct arch_context *context)
{
    context->regs[8] = __NR_restart_syscall;
}

static void setup_syscall_restart(struct arch_context *context, unsigned long restart_addr,
                                  int retval)
{
    switch (retval)
    {
        case -ERR_RESTART_IF_NO_HANDLER:
        case -ERR_RESTART_IF_SIGNAL:
        case -ERR_RESTART_ALWAYS:
        case -ERR_RESTART_WITH_BLOCK:
            context->regs[0] = context->ori_x0;
            context->elr     = restart_addr;
            break;
    }
}

static void restart_to_eintr(struct arch_context *context, unsigned long continue_addr,
                             unsigned long restart_addr, int retval,
                             const struct ksignal *ksig)
{
    if (context->elr == restart_addr &&
        (retval == -ERR_RESTART_IF_NO_HANDLER || retval == -ERR_RESTART_WITH_BLOCK ||
         (retval == -ERR_RESTART_IF_SIGNAL && !(ksig->ka.sa_flags & SA_RESTART))))
    {
        context->regs[0] = -EINTR;
        context->elr     = continue_addr;
    }
}

/* ==================== 信号返回 (rt_sigreturn) ==================== */

/**
 * parse_signal_frame_records - 解析信号帧 __reserved[] 区域中的扩展上下文记录
 * @user: 输出：各扩展上下文的指针与尺寸
 * @signal_frame:   用户态信号帧
 *
 * __reserved[] 是一个变长记录序列，每条记录以 struct _aarch64_ctx（magic + size）
 * 为头，magic == 0 表示终止。支持通过 extra_context 扩展到帧外部。
 *
 * 返回 0 表示解析成功；返回 -EINVAL 表示帧格式非法。
 */
static int parse_signal_frame_records(struct signal_frame_user_contexts *context,
                                      struct signal_frame *signal_frame)
{
    uint32_t magic;
    uint32_t size;
    size_t offset = 0;
    uint32_t head_size = sizeof(struct _aarch64_ctx);
    struct _aarch64_ctx *head;
    struct sigcontext *uc_mcontext = &signal_frame->uc.uc_mcontext;
    char *reserved_base = (char *)&uc_mcontext->__reserved;
    size_t reserved_size  = sizeof(uc_mcontext->__reserved);
    bool have_extra_context = false;

    if (!IS_ALIGNED((unsigned long)reserved_base, AARCH64_SIGFRAME_ALIGN))
    {
        return -EINVAL;
    }

    while (1)
    {
        uint32_t remain_size = reserved_size - offset;
        if (remain_size < head_size)
        {
            return -EINVAL;
        }

        if (!IS_ALIGNED(offset, AARCH64_SIGFRAME_ALIGN))
        {
            return -EINVAL;
        }

        head  = (struct _aarch64_ctx *)(reserved_base + offset);

        magic = head->magic;
        size  = head->size;

        if (remain_size < size)
        {
            return -EINVAL;
        }

        /* end */
        if (!magic && !size)
        {
            return 0;
        }

        /* 每个帧的结构为 head{.magic; .size}+payload 大小等于head.size,所以size不能小于head_size */
        if (size < head_size)
        {
            return -EINVAL;
        }

        switch (magic)
        {
            case 0:
                /* magic为0，size不为0 */
                return -EINVAL;

            case FPSIMD_MAGIC:
                if (!AARCH64_SIGNAL_HAS_FPSIMD || context->fpsimd)
                {
                    return -EINVAL;
                }

                context->fpsimd      = (struct fpsimd_context *)head;
                context->fpsimd_size = size;
                break;

            case ESR_MAGIC:
                /* 忽略 ESR 记录（仅供调试器使用） */
                break;

            case EXTRA_MAGIC:
                KLOG_EMERG("EXTRA_MAGIC unsupported!!!");
                return -EINVAL;

            default:
                KLOG_EMERG("unsupported signal magic:%d\n", magic);
                return -EINVAL;
        }

        if (reserved_size - offset < size)
        {
            return -EINVAL;
        }

        offset += size;
    }

    return 0;
}

static void signal_frame_restore_mcontext(struct arch_context *context,
                                          const struct signal_frame *sigframe)
{
    memcpy(&context->regs, &sigframe->uc.uc_mcontext.regs, sizeof(context->regs));
    context->sp   = sigframe->uc.uc_mcontext.sp;
    context->elr  = sigframe->uc.uc_mcontext.pc;
    context->cpsr = sigframe->uc.uc_mcontext.pstate;
}

static int restore_fpsimd_context_from_sigframe(const struct signal_frame_user_contexts *user,
                                                struct arch_context *context)
{
    if (user->fpsimd_size != sizeof(struct fpsimd_context))
    {
        return -EINVAL;
    }

    if (copy_from_user(&context->fpContext.vRegs, &user->fpsimd->vregs,
                       sizeof(context->fpContext.vRegs)))
    {
        return -EFAULT;
    }

    context->fpContext.fpsr = user->fpsimd->fpsr;
    context->fpContext.fpcr = user->fpsimd->fpcr;

    return 0;
}

/**
 * restore_context_from_signal_frame - 从信号帧恢复用户态寄存器上下文
 * @context: 内核寄存器上下文（目标）
 * @signal_frame:   用户态信号帧（来源）
 */
static int restore_context_from_signal_frame(struct arch_context *context,
                                             struct signal_frame *signal_frame)
{
    struct signal_frame_user_contexts user_context = {0};
    k_sigset_t set;
    int ret;

    /* 恢复信号屏蔽字 */
    ret = copy_from_user(&set, &signal_frame->uc.uc_sigmask, sizeof(set));
    if (ret)
    {
        return -EINVAL;
    }

    signal_mask_apply_current(&set);

    /* 恢复通用寄存器 */
    signal_frame_restore_mcontext(context, signal_frame);
    clear_syscall_context(context);

    if (!aarch64_validate_return_context(context))
    {
        return -EINVAL;
    }

    ret = parse_signal_frame_records(&user_context, signal_frame);
    if (ret)
    {
        return ret;
    }

    /* 恢复 FPSIMD / SVE 上下文 */
    if (AARCH64_SIGNAL_HAS_FPSIMD)
    {
        if (!user_context.fpsimd)
        {
            return -EINVAL;
        }

        ret = restore_fpsimd_context_from_sigframe(&user_context, context);
        if (ret)
        {
            return ret;
        }
    }

    return 0;
}

/**
 * rt_sigreturn - 从信号处理函数返回，恢复被中断时的寄存器上下文
 * @context: 当前寄存器上下文（sp 指向信号帧）
 *
 * 由 restorer trampoline 通过 rt_sigreturn 系统调用调用。
 * 帧校验失败时向当前线程投递 SIGSEGV。
 */
int rt_sigreturn(struct arch_context *context)
{
    struct signal_frame *frame;

    /* AArch64 ABI 要求信号帧 16 字节对齐 */
    if (!IS_ALIGNED(context->sp, AARCH64_SIGFRAME_ALIGN))
    {
        goto error;
    }

    frame = (struct signal_frame *)context->sp;

    if (!user_access_check(frame, sizeof(*frame), UACCESS_R))
    {
        goto error;
    }

    if (restore_context_from_signal_frame(context, frame))
    {
        goto error;
    }


    if (altstack_restore_from_user(&frame->uc.uc_stack, context->sp))
    {
        goto error;
    }

    return 0;

error:
    KLOG_EMERG("rt_sigreturn: invalid signal frame, sending SIGSEGV");
    kernel_signal_send(ttosProcessSelf()->taskControlId->tid, TO_THREAD, SIGSEGV, SI_KERNEL, 0);
    return 0;
}

/* ==================== 架构信号入口 ==================== */

/**
 * arch_do_signal - 信号处理主入口（从异常/系统调用返回路径调用）
 * @regs: 用户态寄存器上下文
 *
 * 流程：
 *  1. 若从系统调用返回，保存重启信息并将 ELR 暂时回退到系统调用指令
 *  2. 取出待处理信号（signal_fetch_deliverable）
 *  3. 根据 retval 和 sa_flags 决定是否将重启转换为 -EINTR
 *  4. 有信号：进入 enter_signal_handler（不返回）
 *  5. 无信号：处理 restart_syscall，恢复可能被临时替换的信号屏蔽字
 */
int arch_do_signal(struct arch_context *context)
{
    int retval = 0;
    unsigned long continue_addr = 0;
    unsigned long restart_addr  = 0;
    struct ksignal ksig = {0};
    bool is_in_syscall = in_syscall(context);

    if (is_in_syscall)
    {
        syscall_restart_addrs(context, &continue_addr, &restart_addr);
        retval = context->regs[0];

        clear_syscall_context(context);

        setup_syscall_restart(context, restart_addr, retval);
    }

    if (signal_fetch_deliverable(&ksig))
    {
        /*
         * 有信号处理函数：若系统调用本应重启，根据 retval 和 SA_RESTART
         * 决定是否将重启撤销为 -EINTR。
         */
        restart_to_eintr(context, continue_addr, restart_addr, retval, &ksig);

        enter_signal_handler(&ksig, context);
        /* enter_signal_handler 调用 restore_raw_context，不会返回到此处 */
        assert(0);
    }

    /* 无信号处理函数：检查是否需要调用__NR_restart_syscall来重启 */
    if (is_in_syscall && context->elr == restart_addr &&
        retval == -ERR_RESTART_WITH_BLOCK)
    {
        /* 改为 __NR_restart_syscall 系统调用号，用户态路径不变 */
        install_restart_syscall(context);
    }

    /* 恢复被 sigsuspend/sigtimedwait 等临时替换的信号屏蔽字 */
    signal_mask_restore_saved();

    return 0;
}
