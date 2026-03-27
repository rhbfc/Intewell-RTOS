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

#ifndef __TTOS_PROCESS_H__
#define __TTOS_PROCESS_H__

#include <errno.h>
#include <completion.h>
#include <fs/fs.h>
#include <futex.h>
#include <pgroup.h>
#include <process_obj.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttosRBTree.h>
#include <tty.h>
#include <restart.h>

#ifndef __user
#define __user
#endif

#define INVALID_PROCESS_ID ((pid_t)-1)

#define FORKNOEXEC 0x00000001
#define SIGPENDING 0x00000002

/* Used in pcb->exit_state: */
#define EXIT_DEAD 0x00000010
#define EXIT_ZOMBIE 0x00000020

#define PROCESS_CREATE_STAT_EXIT(exit_code) (((exit_code)&0xff) << 8)
#define PROCESS_CREATE_STAT_SIGNALED(signo) ((signo) & 0x7f)
#define PROCESS_CREATE_STAT_STOPPED(signo) (PROCESS_CREATE_STAT_EXIT(signo) | 0x7f)
#define PROCESS_CREATE_STAT_CONTINUED (0xffff)

struct waitpid_info
{
    T_TTOS_Task_Queue_Control waitQueue;
    /* 保留退出的子进程，父进程将其从僵尸态释放 */
    pcb_t waker;
    int status;
};

struct T_TTOS_ProcessControlBlock
{
    struct arch_context exception_context;
    struct arch_context *exception_context_raw_ptr;
    TASK_ID taskControlId;
    struct T_TTOS_ProcessControlBlock *parent;
    struct T_TTOS_ProcessControlBlock *first_child;
    struct T_TTOS_ProcessControlBlock *sibling;
    struct process_obj *pid;
    pcb_t group_leader;
    struct pgroup *pgrp;
    struct process_obj *mm;
    struct process_obj *vfs_list;
    struct process_obj *signal;
    struct process_obj *sighand;
    struct process_obj *wait_info;
    /* 当前生效的信号屏蔽字 */
    k_sigset_t blocked;
    /* 信号处理期间临时扩展后的实际屏蔽字 */
    k_sigset_t real_blocked;
    /* 需要延后恢复的旧屏蔽字 */
    k_sigset_t saved_sigmask;
    /* 备用信号栈的基地址 */
    unsigned long altstack_base;
    /* 备用信号栈的大小 */
    size_t altstack_size;
    /* 备用信号栈状态标志，如 SS_DISABLE/SS_AUTODISARM */
    unsigned int altstack_flags;
    struct process_obj *cmdline;
    phys_addr_t auxvp;
    struct process_obj *envp;
    int (*entry)(void *);
    void *args;
    void *userStack;
    T_ULONG tls;

    struct futex_q futex;

    int __user *set_child_tid;
    int __user *clear_child_tid;

    int exit_state;
    int exit_code;
    int exit_signal;

    T_TTOS_CompletionControl *vfork_done;
    struct list_head thread_group;
    struct list_head sibling_node;
    struct list_head pgrp_node;
    struct list_head obj_list;
    struct list_head sched_group_node;
    struct list_head non_auto_start_node;
    struct timespec64 utime;
    struct timespec64 utime_prev;
    struct timespec64 stime;
    struct timespec64 stime_prev;
    struct timespec64 start_time;
    struct k_sigpending_queue sig_queue;
    unsigned int personality;
    /* 启动进程的用户 */
    uid_t uid;
    /* 用于判断权限的uid 对于带有setuid位的二进制文件启动时更改 */
    uid_t euid;
    /* 用于保存变化前的 euid */
    uid_t suid;
    /* 启动进程的用户组 */
    uid_t gid;
    /* 用于判断权限的egid 对于带有setgid位的二进制文件启动时更改 */
    uid_t egid;
    /* 用户保存变化前的egid */
    uid_t sgid;

    pid_t tgid;
    pid_t pgid;
    struct
    {
        bool is_terminated : 1;
        bool group_request_terminate : 1;
        bool is_normal_exit : 1;
        uint8_t padding : 5;
        uint32_t exit_code : 24;
    } group_exit_status;
    pid_t sid;
    mode_t umask;
    int need_restore_blocked;
    uint32_t aux_len;
    unsigned int jobctl_stopped;
    unsigned int wait_reap_stp;
    T_UWORD state;
    int sched_group_id;
    int flags;
    ttos_spinlock_t lock;
    ttos_spinlock_t tglock;
    char cmd_name[NAME_MAX];
    char cmd_path[PATH_MAX];
    char pwd[PATH_MAX];
    char root[PATH_MAX];
    unsigned long jobctl;
    struct restart_info restart;
};

static inline void pcb_flags_set(pcb_t pcb, int mask)
{
    __atomic_fetch_or(&pcb->flags, mask, __ATOMIC_RELEASE);
}

static inline void pcb_flags_clear(pcb_t pcb, int mask)
{
    __atomic_fetch_and(&pcb->flags, ~mask, __ATOMIC_RELEASE);
}

static inline bool pcb_flags_test(pcb_t pcb, int mask)
{
    return pcb && ((__atomic_load_n(&pcb->flags, __ATOMIC_ACQUIRE) & mask) != 0);
}

#define pcb_get(pcb) ttosObjectGet(&((pcb_t)(pcb))->taskControlId->objCore)
#define pcb_put(pcb) ttosObjectPut(&((pcb_t)(pcb))->taskControlId->objCore)
#define pcb_wait_to_free(pcb)                                                                      \
    ttosObjectWaitAndSetToFree(&((pcb_t)(pcb))->taskControlId->objCore, FALSE)

pcb_t ttosProcessSelf(void);
void process_destroy(pcb_t pcb);
int do_execve(const char *filename, const char *const *argv, const char *const *envp);

struct process_obj *pid_obj_alloc(pcb_t pcb);
int pid_obj_ref(pcb_t parent, pcb_t child);
pcb_t pcb_get_by_pid(pid_t pid);
pcb_t pcb_get_by_pid_nt(pid_t pid);

void process_filelist_copy(pcb_t parent, pcb_t child);
void process_filelist_ref(pcb_t parent, pcb_t child);
void process_filelist_create(pcb_t pcb);

void process_mm_ref(pcb_t parent, pcb_t child);
void process_mm_create(pcb_t child);
void process_mm_copy(pcb_t parent, pcb_t child);
phys_addr_t process_mm_map(pcb_t pcb, phys_addr_t paddr, virt_addr_t *vaddr, uintptr_t attr,
                           size_t page_count, int flags);
int process_mm_unmap(pcb_t pcb, virt_addr_t vaddr);
int process_mremap(virt_addr_t addr, unsigned long old_len, unsigned long new_len,
                   unsigned long flags, virt_addr_t *new_addr);
long process_exit_group(int flag, bool is_normal_exit);
void process_exit(pcb_t pcb);

void vfork_exec_wake(pcb_t pcb);
void vfork_exit_wake(pcb_t pcb);

void foreach_process_child(pcb_t pcb, void (*func)(pcb_t, void *), void *param);
void foreach_task_group(pcb_t pcb, void (*func)(pcb_t, void *), void *param);

void process_foreach(void (*func)(pcb_t, void *), void *arg);
pid_t kernel_execve(const char *filename, const char *const *argv, const char *const *envp);
struct filelist *pcb_get_files(pcb_t pcb);
pid_t pid_obj_get_pid(struct process_obj *obj);
char *process_getfullpath(int dirfd, const char __user *path);
char *process_getfilefullpath(struct file *f, const char __user *path);
void pcb_set_pid_leader(pcb_t pcb);

void process_wait_info_create(pcb_t pcb);
void process_wait_info_ref(pcb_t parent, pcb_t child);
void process_wakeup_waiter(pcb_t pcb);
int process_release_zombie(pcb_t pcb);

struct file *process_getfile(pcb_t pcb, int fd);
uint64_t process_prot_to_attr(unsigned long prot);
int process_mmap(pcb_t pcb, unsigned long *addr, unsigned long len, unsigned long prot,
                 unsigned long flags, struct file *f, off_t off, int file_len);
#define get_process_mm(pcb) PROCESS_OBJ_GET((pcb)->mm, struct mm *)
#define get_process_vfs_list(pcb) PROCESS_OBJ_GET((pcb)->vfs_list, struct filelist *)
#define get_process_pid(pcb) pid_obj_get_pid((pcb)->pid)

#define get_process_waitinfo(pcb) PROCESS_OBJ_GET((pcb)->wait_info, struct waitpid_info *)

#define get_process_sighand(pcb) PROCESS_OBJ_GET((pcb)->sighand, struct shared_sighand *)

#define get_process_signal(pcb) PROCESS_OBJ_GET((pcb)->signal, struct shared_signal *)

#define get_process_ctty(pcb) ((tty_t)get_process_signal(pcb)->ctty)

#define for_each_thread_in_tgroup(pcb, tmp_pcb)                                                    \
    list_for_each_entry(tmp_pcb, &pcb->group_leader->thread_group, sibling_node)
pid_t do_fork(unsigned long clone_flags, unsigned long newsp, int __user *set_child_tid,
              int __user *clear_child_tid, unsigned long tls, struct period_param *param);

/* 兼容linux的 current */
#define current (ttosProcessSelf())

#endif /* __TTOS_PROCESS_H__ */
