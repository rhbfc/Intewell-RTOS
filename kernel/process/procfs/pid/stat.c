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
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/procfs.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time/ktime.h>
#include <ttosProcess.h>

/**
 * @brief /proc/[pid]/stat
 * Status information about the process.  This is used by ps(1). It
 * is defined in the kernel source file fs/proc/array.c.
 *
 * The fields, in order, with their proper scanf(3) format specifiers, are
 * listed below. Some Linux-specific permission checks are not modeled here,
 * so protected fields are emitted using the data currently visible to the
 * kernel.
 *
 * (1) pid  %d
 *        The process ID.
 *
 * (2) comm  %s
 *        The filename of the executable, in parentheses.  Strings
 *        longer than TASK_COMM_LEN (16) characters (including the terminating
 *        null byte) are silently truncated.  This is visible whether or not the
 *        executable is  swapped out.
 *
 * (3) state  %c
 *        One of the following characters, indicating process
 *        state:
 *
 *        R  Running
 *
 *        S  Sleeping in an interruptible wait
 *
 *        D  Waiting in uninterruptible disk sleep
 *
 *        Z  Zombie
 *
 *        T  Stopped (on a signal) or (before Linux 2.6.33) trace
 *           stopped
 *
 *        t  Tracing stop (Linux 2.6.33 onward)
 *
 *        W  Paging (only before Linux 2.6.0)
 *
 *        X  Dead (from Linux 2.6.0 onward)
 *
 *        x  Dead (Linux 2.6.33 to 3.13 only)
 *
 *        K  Wakekill (Linux 2.6.33 to 3.13 only)
 *
 *        W  Waking (Linux 2.6.33 to 3.13 only)
 *
 *        P  Parked (Linux 3.9 to 3.13 only)
 *
 * (4) ppid  %d
 *        The PID of the parent of this process.
 *
 * (5) pgrp  %d
 *        The process group ID of the process.
 *
 * (6) session  %d
 *        The session ID of the process.
 *
 * (7) tty_nr  %d
 *        The controlling terminal of the process.  (The minor
 *        device number is contained in the combination of bits 31 to 20 and 7
 *        to 0; the major device number is in bits 15 to 8.)
 *
 * (8) tpgid  %d
 *        The ID of the foreground process group of the controlling
 *        terminal of the process.
 *
 * (9) flags  %u
 *        The kernel flags word of the process.  For bit meanings,
 *        see the PF_* defines in the Linux kernel source file
 *        include/linux/sched.h. Details depend on the kernel version.
 *
 *        The format for this field was %lu before Linux 2.6.
 *
 * (10) minflt  %lu
 *        The number of minor faults the process has made which
 *        have not required loading a memory page from disk.
 *
 * (11) cminflt  %lu
 *        The number of minor faults that the process's waited-for
 *        children have made.
 *
 * (12) majflt  %lu
 *        The number of major faults the process has made which
 *        have required loading a memory page from disk.
 *
 * (13) cmajflt  %lu
 *        The number of major faults that the process's waited-for
 *        children have made.
 *
 * (14) utime  %lu
 *        Amount  of  time  that this process has been scheduled in
 *        user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
 *        This includes guest time, guest_time (time spent running a virtual
 *        CPU, see below), so that applications that are not aware of the guest
 *        time field do not lose that time from their calculations.
 *
 * (15) stime  %lu
 *        Amount of time that this process has been scheduled in
 *        kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
 *
 * (16) cutime  %ld
 *        Amount of time that this process's waited-for children
 *        have been scheduled in user mode, measured in clock ticks (divide by
 *        sysconf(_SC_CLK_TCK)).  (See also times(2).)  This includes guest
 *        time, cguest_time (time spent running a virtual CPU, see below).
 *
 * (17) cstime  %ld
 *        Amount of time that this process's waited-for children
 *        have been scheduled in kernel mode, measured in clock ticks (divide by
 *        sysconf(_SC_CLK_TCK)).
 *
 * (18) priority  %ld
 *        (Explanation  for  Linux 2.6) For processes running a
 *        real-time scheduling policy (policy below; see sched_setscheduler(2)),
 *        this is the negated scheduling priority, minus one; that is, a number
 *        in the range -2 to -100, corresponding to real-time priorities 1
 *        to 99.  For processes running under a non-real-time scheduling policy,
 *        this is the raw nice value (setpriority(2)) as represented in the
 *        kernel. The kernel stores nice values as numbers in the range 0 (high)
 *        to 39 (low), corresponding to the user-visible nice range of -20
 *        to 19.
 *
 *        Before Linux 2.6, this was a scaled value based on the
 *        scheduler weighting given to this process.
 *
 * (19) nice  %ld
 *        The nice value (see setpriority(2)), a value in the range
 *        19 (low priority) to -20 (high priority).
 *
 * (20) num_threads  %ld
 *        Number of threads in this process (since Linux 2.6).
 *        Before kernel 2.6, this field was hard coded to 0 as a placeholder for
 *        an earlier removed field.
 *
 * (21) itrealvalue  %ld
 *        The time in jiffies before the next SIGALRM is sent to
 *        the process due to an interval timer.  Since kernel 2.6.17, this field
 *        is no longer maintained, and is hard coded as 0.
 *
 * (22) starttime  %llu
 *        The time the process started after system boot.  In
 *        kernels before Linux 2.6, this value was expressed in jiffies.  Since
 *        Linux 2.6, the value is expressed in clock ticks (divide by
 *        sysconf(_SC_CLK_TCK)).
 *
 *        The format for this field was %lu before Linux 2.6.
 *
 * (23) vsize  %lu
 *        Virtual memory size in bytes.
 *
 * (24) rss  %ld
 *        Resident  Set  Size: number of pages the process has in
 *        real memory.  This is just the pages which count toward text, data, or
 *        stack space.  This does not include pages which have not been
 *        demand-loaded in, or which are swapped out.  This value is inaccurate;
 *        see /proc/[pid]/statm below.
 *
 * (25) rsslim  %lu
 *        Current soft limit in bytes on the rss of the process;
 *        see the description of RLIMIT_RSS in getrlimit(2).
 *
 * (26) startcode  %lu  [PT]
 *        The address above which program text can run.
 *
 * (27) endcode  %lu  [PT]
 *        The address below which program text can run.
 *
 * (28) startstack  %lu  [PT]
 *        The address of the start (i.e., bottom) of the stack.
 *
 * (29) kstkesp  %lu  [PT]
 *        The current value of ESP (stack pointer), as found in the
 *        kernel stack page for the process.
 *
 * (30) kstkeip  %lu  [PT]
 *        The current EIP (instruction pointer).
 *
 * (31) signal  %lu
 *        The bitmap of pending signals, displayed as a decimal
 *        number.  Obsolete, because it does not provide information on
 *        real-time signals; use /proc/[pid]/status instead.
 *
 * (32) blocked  %lu
 *        The bitmap of blocked signals, displayed as a decimal
 *        number.  Obsolete, because it does not provide information on
 *        real-time signals; use /proc/[pid]/status instead.
 *
 * (33) sigignore  %lu
 *        The bitmap of ignored signals, displayed as a decimal
 *        number.  Obsolete, because it does not provide information on
 *        real-time signals; use /proc/[pid]/status instead.
 *
 * (34) sigcatch  %lu
 *        The bitmap of caught signals, displayed as a decimal
 *        number.  Obsolete, because it does not provide information on
 *        real-time signals; use /proc/[pid]/status instead.
 *
 * (35) wchan  %lu  [PT]
 *        This is the "channel" in which the process is waiting. It
 *        is the address of a location in the kernel where the process is
 *        sleeping. The corresponding symbolic name can be found in
 *        /proc/[pid]/wchan.
 *
 * (36) nswap  %lu
 *        Number of pages swapped (not maintained).
 *
 * (37) cnswap  %lu
 *        Cumulative nswap for child processes (not maintained).
 *
 * (38) exit_signal  %d  (since Linux 2.1.22)
 *        Signal to be sent to parent when we die.
 *
 * (39) processor  %d  (since Linux 2.2.8)
 *        CPU number last executed on.
 *
 * (40) rt_priority  %u  (since Linux 2.5.19)
 *        Real-time scheduling priority, a number in the range 1 to
 *        99 for processes scheduled under a real-time policy, or 0, for
 *        non-real-time processes (see sched_setscheduler(2)).
 *
 * (41) policy  %u  (since Linux 2.5.19)
 *        Scheduling policy (see sched_setscheduler(2)).  Decode
 *        using the SCHED_* constants in linux/sched.h.
 *
 *        The format for this field was %lu before Linux 2.6.22.
 *
 * (42) delayacct_blkio_ticks  %llu  (since Linux 2.6.18)
 *        Aggregated block I/O delays, measured in clock ticks
 *        (centiseconds).
 *
 * (43) guest_time  %lu  (since Linux 2.6.24)
 *        Guest time of the process (time spent running a virtual
 *        CPU for a guest operating system), measured in clock ticks (divide by
 *        sysconf(_SC_CLK_TCK)).
 *
 * (44) cguest_time  %ld  (since Linux 2.6.24)
 *        Guest time of the process's children, measured in clock
 *        ticks (divide by sysconf(_SC_CLK_TCK)).
 *
 * (45) start_data  %lu  (since Linux 3.3)  [PT]
 *        Address above which program initialized and uninitialized
 *       (BSS) data are placed.
 *
 * (46) end_data  %lu  (since Linux 3.3)  [PT]
 *        Address below which program initialized and uninitialized
 * (BSS) data are placed.
 *
 * (47) start_brk  %lu  (since Linux 3.3)  [PT]
 *        Address above which program heap can be expanded with
 *        brk(2).
 *
 * (48) arg_start  %lu  (since Linux 3.5)  [PT]
 *        Address above which program command-line arguments (argv)
 *        are placed.
 *
 * (49) arg_end  %lu  (since Linux 3.5)  [PT]
 *        Address below program command-line arguments (argv) are
 *        placed.
 *
 * (50) env_start  %lu  (since Linux 3.5)  [PT]
 *        Address above which program environment is placed.
 *
 * (51) env_end  %lu  (since Linux 3.5)  [PT]
 *        Address below which program environment is placed.
 *
 * (52) exit_code  %d  (since Linux 3.5)  [PT]
 *        The thread's exit status in the form reported by
 *        waitpid(2).
 *
 */

#define LINELEN 256
struct pcb_stat_s
{
    int pid;
    char comm[NAME_MAX];
    char state;
    int ppid;
    int pgrp;
    int session;
    int tty_nr;
    int tpgid;
    unsigned int flags;
    unsigned long minflt;
    unsigned long cminflt;
    unsigned long majflt;
    unsigned long cmajflt;
    unsigned long utime;
    unsigned long stime;
    long cutime;
    long cstime;
    long priority;
    long nice;
    long num_threads;
    long itrealvalue;
    unsigned long long starttime;
    unsigned long vsize;
    long rss;
    unsigned long rsslim;
    unsigned long startcode;
    unsigned long endcode;
    unsigned long startstack;
    unsigned long kstkesp;
    unsigned long kstkeip;
    unsigned long signal;
    unsigned long blocked;
    unsigned long sigignore;
    unsigned long sigcatch;
    unsigned long wchan;
    unsigned long nswap;
    unsigned long cnswap;
    int exit_signal;
    int processor;
    unsigned int rt_priority;
    unsigned int policy;
    unsigned long long delayacct_blkio_ticks;
    unsigned long guest_time;
    long cguest_time;
    unsigned long start_data;
    unsigned long end_data;
    unsigned long start_brk;
    unsigned long arg_start;
    unsigned long arg_end;
    unsigned long env_start;
    unsigned long env_end;
    int exit_code;
};
T_UBYTE corePriorityToPthread(int priority);

static void task_enum(pcb_t pcb, void *param)
{
    struct pcb_stat_s *stat = param;
    stat->num_threads++;
    stat->utime +=
        pcb->utime.tv_sec * MUSL_SC_CLK_TCK + pcb->utime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
    stat->stime +=
        pcb->stime.tv_sec * MUSL_SC_CLK_TCK + pcb->stime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
}
static void mm_region_enum(struct mm *mm, struct mm_region *region, void *ctx)
{
    struct pcb_stat_s *stat = ctx;
    stat->vsize += region->region_page_count * ttosGetPageSize();
}

static char get_task_status(TASK_ID task)
{
    if (task->smpInfo.cpuIndex == task->smpInfo.last_running_cpu)
    {
        return 'R';
    }
    return 'S';
}

static int task_stat_get(struct pcb_stat_s *stat, TASK_ID task)
{
    memset(stat, 0, sizeof(*stat));
    stat->pid = task->tid;
    strlcpy(stat->comm, (char *)task->objCore.objName, sizeof stat->comm);
    stat->state = get_task_status(task); // todo

    stat->num_threads = 1;

    uintptr_t total_nr;
    uintptr_t free_nr;
    ttosPageGetInfo(&total_nr, &free_nr);

    stat->rsslim = free_nr * ttosGetPageSize();

    stat->stime = task->stime.tv_sec * MUSL_SC_CLK_TCK +
                  task->stime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
    stat->cstime = stat->stime;
    stat->startstack = (unsigned long)task->stackBottom;
    stat->priority = corePriorityToPthread(task->taskCurPriority);
    stat->nice = 0;
    stat->rt_priority = stat->priority;
    stat->policy = 1; // todo
    stat->processor = task->smpInfo.last_running_cpu;
    return 0;
}

static int pcb_stat_get(struct pcb_stat_s *stat, pcb_t pcb, bool is_thread)
{
    pcb_t parent = pcb->group_leader->parent;

    stat->pid = pcb->taskControlId->tid;
    strlcpy(stat->comm, pcb->cmd_name, sizeof stat->comm);
    stat->state = get_task_status(pcb->taskControlId);
    stat->ppid = parent ? get_process_pid(parent) : 0;
    stat->pgrp = pcb->pgid;
    stat->session = pcb->sid;
    stat->tty_nr = 0;                        // todo
    stat->tpgid = pcb->sid;                  // todo
    stat->flags = pcb->taskControlId->state; // todo
    stat->minflt = 0;
    stat->cminflt = 0;
    stat->majflt = 0;
    stat->cmajflt = 0;

    if (is_thread)
    {
        stat->num_threads = 1;
        stat->utime = pcb->utime.tv_sec * MUSL_SC_CLK_TCK +
                      pcb->utime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
        stat->stime = pcb->stime.tv_sec * MUSL_SC_CLK_TCK +
                      pcb->stime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
    }
    else
    {
        stat->utime = 0;
        stat->stime = 0;
        stat->num_threads = 0;
        foreach_task_group(pcb, task_enum, stat);
    }
    stat->vsize = 0;
    mm_foreach_region(get_process_mm(pcb), mm_region_enum, stat);

    /* 由于实时系统不存在缺页异常， 所有内存均已映射 故而此处 rss与vsize相等 */
    stat->rss = stat->vsize / ttosGetPageSize();

    uintptr_t total_nr;
    uintptr_t free_nr;
    ttosPageGetInfo(&total_nr, &free_nr);

    stat->rsslim = free_nr * ttosGetPageSize();

    stat->cutime = stat->utime; // 未计算子进程
    stat->cstime = stat->stime; // 未计算子进程
    stat->priority = corePriorityToPthread(pcb->taskControlId->taskCurPriority);
    stat->nice = 0;

    stat->itrealvalue = 0; // todo
    stat->starttime = pcb->start_time.tv_sec * MUSL_SC_CLK_TCK +
                      pcb->start_time.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);

    stat->startcode = 0; // todo
    stat->endcode = 0;   // todo
    stat->startstack = (unsigned long)pcb->userStack;
    stat->kstkesp = 0;   // todo
    stat->kstkeip = 0;   // todo
    stat->signal = 0;    // todo
    stat->blocked = 0;   // todo
    stat->sigignore = 0; // todo
    stat->sigcatch = 0;  // todo
    stat->wchan = 0;     // todo
    stat->nswap = 0;
    stat->cnswap = 0;
    stat->exit_signal = 0;
    stat->processor = pcb->taskControlId->smpInfo.last_running_cpu;
    stat->rt_priority = stat->priority;
    stat->policy = 1;                // todo
    stat->delayacct_blkio_ticks = 0; // todo
    stat->guest_time = 0;            // todo
    stat->cguest_time = 0;           // todo
    stat->start_data = 0;            // todo
    stat->end_data = 0;              // todo
    stat->start_brk = 0;             // todo
    stat->arg_start = 0;             // todo
    stat->arg_end = 0;               // todo
    stat->env_start = 0;             // todo
    stat->env_end = 0;               // todo
    stat->exit_code = 0;             // todo

    return 0;
}
struct seq_data
{
    char buf[4096];
    struct proc_seq_buf seq;
};
static int proc_file_open(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = calloc(1, sizeof(struct seq_data));
    if (!data)
    {
        return -ENOMEM;
    }
    *(struct seq_data **)buffer = data;
    return 0;
}
static int proc_file_release(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = buffer;
    if (data)
    {
        free(data);
    }
    return 0;
}
static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    TASK_ID task;
    pcb_t pcb = pcb_get_by_pid_nt(inode->pid);
    if (pcb == NULL)
    {
        // task
        task = task_get_by_tid(inode->pid);
        if (task == NULL)
        {
            return -ENOENT;
        }
        pcb = task->ppcb;
    }

    if (*ppos == 0)
    {
        struct pcb_stat_s stat;
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);
        if (pcb == NULL)
        {
            task_stat_get(&stat, task);
        }
        else
        {
            pcb_stat_get(&stat, pcb, pcb != pcb->group_leader);
        }

        proc_seq_printf(&data->seq,
                        "%d (%s) %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld "
                        "%ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                        "%lu %d %d %u %u %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %d\n",
                        stat.pid, stat.comm, stat.state, stat.ppid, stat.pgrp, stat.session,
                        stat.tty_nr, stat.tpgid, stat.flags, stat.minflt, stat.cminflt, stat.majflt,
                        stat.cmajflt, stat.utime, stat.stime, stat.cutime, stat.cstime,
                        stat.priority, stat.nice, stat.num_threads, stat.itrealvalue,
                        stat.starttime, stat.vsize, stat.rss, stat.rsslim, stat.startcode,
                        stat.endcode, stat.startstack, stat.kstkesp, stat.kstkeip, stat.signal,
                        stat.blocked, stat.sigignore, stat.sigcatch, stat.wchan, stat.nswap,
                        stat.cnswap, stat.exit_signal, stat.processor, stat.rt_priority,
                        stat.policy, stat.delayacct_blkio_ticks, stat.guest_time, stat.cguest_time,
                        stat.start_data, stat.end_data, stat.start_brk, stat.arg_start,
                        stat.arg_end, stat.env_start, stat.env_end, stat.exit_code);
    }

    return proc_seq_read(&data->seq, buffer, count);
}

static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};

int create_pid_stat(pid_t pid, struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = proc_create("stat", 0444, parent, &ops);
    entry->pid = pid;
    return 0;
}
