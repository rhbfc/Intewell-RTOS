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

#include "syscall_internal.h"
#include <period_sched_group.h>
#include <syscalls.h>
#include <ttos.h>
#include <trace.h>

static const int syscall_argc_table[] = {
#if defined(__NR_restart_syscall)
    SYSCALL_ARGC_ITEM (restart_syscall),
#endif 

#if defined(__NR_exit)
    SYSCALL_ARGC_ITEM (exit),
#endif 

#if defined(__NR_fork)
    SYSCALL_ARGC_ITEM (fork),
#endif 

#if defined(__NR_read)
    SYSCALL_ARGC_ITEM (read),
#endif 

#if defined(__NR_write)
    SYSCALL_ARGC_ITEM (write),
#endif 

#if defined(__NR_open)
    SYSCALL_ARGC_ITEM (open),
#endif 

#if defined(__NR_close)
    SYSCALL_ARGC_ITEM (close),
#endif 

#if defined(__NR_waitpid)
    SYSCALL_ARGC_ITEM (waitpid),
#endif 

#if defined(__NR_creat)
    SYSCALL_ARGC_ITEM (creat),
#endif 

#if defined(__NR_link)
    SYSCALL_ARGC_ITEM (link),
#endif 

#if defined(__NR_unlink)
    SYSCALL_ARGC_ITEM (unlink),
#endif 

#if defined(__NR_execve)
    SYSCALL_ARGC_ITEM (execve),
#endif 

#if defined(__NR_chdir)
    SYSCALL_ARGC_ITEM (chdir),
#endif 

#if defined(__NR_time)
    SYSCALL_ARGC_ITEM (time),
#endif 

#if defined(__NR_mknod)
    SYSCALL_ARGC_ITEM (mknod),
#endif 

#if defined(__NR_chmod)
    SYSCALL_ARGC_ITEM (chmod),
#endif 

#if defined(__NR_lchown)
    SYSCALL_ARGC_ITEM (lchown),
#endif 

#if defined(__NR_break)
    SYSCALL_ARGC_ITEM (break),
#endif 

#if defined(__NR_oldstat)
    SYSCALL_ARGC_ITEM (oldstat),
#endif 

#if defined(__NR_lseek)
    SYSCALL_ARGC_ITEM (lseek),
#endif 

#if defined(__NR_getpid)
    SYSCALL_ARGC_ITEM (getpid),
#endif 

#if defined(__NR_mount)
    SYSCALL_ARGC_ITEM (mount),
#endif 

#if defined(__NR_umount)
    SYSCALL_ARGC_ITEM (umount),
#endif 

#if defined(__NR_setuid)
    SYSCALL_ARGC_ITEM (setuid),
#endif 

#if defined(__NR_getuid)
    SYSCALL_ARGC_ITEM (getuid),
#endif 

#if defined(__NR_stime)
    SYSCALL_ARGC_ITEM (stime),
#endif 

#if defined(__NR_alarm)
    SYSCALL_ARGC_ITEM (alarm),
#endif 

#if defined(__NR_oldfstat)
    SYSCALL_ARGC_ITEM (oldfstat),
#endif 

#if defined(__NR_pause)
    SYSCALL_ARGC_ITEM (pause),
#endif 

#if defined(__NR_utime)
    SYSCALL_ARGC_ITEM (utime),
#endif 

#if defined(__NR_stty)
    SYSCALL_ARGC_ITEM (stty),
#endif 

#if defined(__NR_gtty)
    SYSCALL_ARGC_ITEM (gtty),
#endif 

#if defined(__NR_access)
    SYSCALL_ARGC_ITEM (access),
#endif 

#if defined(__NR_nice)
    SYSCALL_ARGC_ITEM (nice),
#endif 

#if defined(__NR_ftime)
    SYSCALL_ARGC_ITEM (ftime),
#endif 

#if defined(__NR_sync)
    SYSCALL_ARGC_ITEM (sync),
#endif 

#if defined(__NR_kill)
    SYSCALL_ARGC_ITEM (kill),
#endif 

#if defined(__NR_rename)
    SYSCALL_ARGC_ITEM (rename),
#endif 

#if defined(__NR_mkdir)
    SYSCALL_ARGC_ITEM (mkdir),
#endif 

#if defined(__NR_rmdir)
    SYSCALL_ARGC_ITEM (rmdir),
#endif 

#if defined(__NR_dup)
    SYSCALL_ARGC_ITEM (dup),
#endif 

#if defined(__NR_pipe)
    SYSCALL_ARGC_ITEM (pipe),
#endif 

#if defined(__NR_times)
    SYSCALL_ARGC_ITEM (times),
#endif 

#if defined(__NR_prof)
    SYSCALL_ARGC_ITEM (prof),
#endif 

#if defined(__NR_brk)
    SYSCALL_ARGC_ITEM (brk),
#endif 

#if defined(__NR_setgid)
    SYSCALL_ARGC_ITEM (setgid),
#endif 

#if defined(__NR_getgid)
    SYSCALL_ARGC_ITEM (getgid),
#endif 

#if defined(__NR_signal)
    SYSCALL_ARGC_ITEM (signal),
#endif 

#if defined(__NR_geteuid)
    SYSCALL_ARGC_ITEM (geteuid),
#endif 

#if defined(__NR_getegid)
    SYSCALL_ARGC_ITEM (getegid),
#endif 

#if defined(__NR_acct)
    SYSCALL_ARGC_ITEM (acct),
#endif 

#if defined(__NR_umount2)
    SYSCALL_ARGC_ITEM (umount2),
#endif 

#if defined(__NR_lock)
    SYSCALL_ARGC_ITEM (lock),
#endif 

#if defined(__NR_ioctl)
    SYSCALL_ARGC_ITEM (ioctl),
#endif 

#if defined(__NR_fcntl)
    SYSCALL_ARGC_ITEM (fcntl),
#endif 

#if defined(__NR_mpx)
    SYSCALL_ARGC_ITEM (mpx),
#endif 

#if defined(__NR_setpgid)
    SYSCALL_ARGC_ITEM (setpgid),
#endif 

#if defined(__NR_ulimit)
    SYSCALL_ARGC_ITEM (ulimit),
#endif 

#if defined(__NR_oldolduname)
    SYSCALL_ARGC_ITEM (oldolduname),
#endif 

#if defined(__NR_umask)
    SYSCALL_ARGC_ITEM (umask),
#endif 

#if defined(__NR_chroot)
    SYSCALL_ARGC_ITEM (chroot),
#endif 

#if defined(__NR_ustat)
    SYSCALL_ARGC_ITEM (ustat),
#endif 

#if defined(__NR_dup2)
    SYSCALL_ARGC_ITEM (dup2),
#endif 

#if defined(__NR_getppid)
    SYSCALL_ARGC_ITEM (getppid),
#endif 

#if defined(__NR_getpgrp)
    SYSCALL_ARGC_ITEM (getpgrp),
#endif 

#if defined(__NR_setsid)
    SYSCALL_ARGC_ITEM (setsid),
#endif 

#if defined(__NR_sigaction)
    SYSCALL_ARGC_ITEM (sigaction),
#endif 

#if defined(__NR_sgetmask)
    SYSCALL_ARGC_ITEM (sgetmask),
#endif 

#if defined(__NR_ssetmask)
    SYSCALL_ARGC_ITEM (ssetmask),
#endif 

#if defined(__NR_setreuid)
    SYSCALL_ARGC_ITEM (setreuid),
#endif 

#if defined(__NR_setregid)
    SYSCALL_ARGC_ITEM (setregid),
#endif 

#if defined(__NR_sigsuspend)
    SYSCALL_ARGC_ITEM (kernel_signal_suspend),
#endif 

#if defined(__NR_sigpending)
    SYSCALL_ARGC_ITEM (sigpending),
#endif 

#if defined(__NR_sethostname)
    SYSCALL_ARGC_ITEM (sethostname),
#endif 

#if defined(__NR_setrlimit)
    SYSCALL_ARGC_ITEM (setrlimit),
#endif 

#if defined(__NR_getrlimit)
    SYSCALL_ARGC_ITEM (getrlimit),
#endif 

#if defined(__NR_getrusage)
    SYSCALL_ARGC_ITEM (getrusage),
#endif 

#if defined(__NR_gettimeofday_time32)
    SYSCALL_ARGC_ITEM (gettimeofday_time32),
#endif 

#if defined(__NR_gettimeofday)
    SYSCALL_ARGC_ITEM (gettimeofday),
#endif 

#if defined(__NR_settimeofday_time32)
    SYSCALL_ARGC_ITEM (settimeofday_time32),
#endif 

#if defined(__NR_settimeofday)
    SYSCALL_ARGC_ITEM (settimeofday),
#endif 

#if defined(__NR_getgroups)
    SYSCALL_ARGC_ITEM (getgroups),
#endif 

#if defined(__NR_setgroups)
    SYSCALL_ARGC_ITEM (setgroups),
#endif 

#if defined(__NR_select)
    SYSCALL_ARGC_ITEM (select),
#endif 

#if defined(__NR_symlink)
    SYSCALL_ARGC_ITEM (symlink),
#endif 

#if defined(__NR_oldlstat)
    SYSCALL_ARGC_ITEM (oldlstat),
#endif 

#if defined(__NR_readlink)
    SYSCALL_ARGC_ITEM (readlink),
#endif 

#if defined(__NR_uselib)
    SYSCALL_ARGC_ITEM (uselib),
#endif 

#if defined(__NR_swapon)
    SYSCALL_ARGC_ITEM (swapon),
#endif 

#if defined(__NR_reboot)
    SYSCALL_ARGC_ITEM (reboot),
#endif 

#if defined(__NR_readdir)
    SYSCALL_ARGC_ITEM (readdir),
#endif 

#if defined(__NR_mmap)
    SYSCALL_ARGC_ITEM (mmap),
#endif 

#if defined(__NR_munmap)
    SYSCALL_ARGC_ITEM (munmap),
#endif 

#if defined(__NR_truncate)
    SYSCALL_ARGC_ITEM (truncate),
#endif 

#if defined(__NR_ftruncate)
    SYSCALL_ARGC_ITEM (ftruncate),
#endif 

#if defined(__NR_fchmod)
    SYSCALL_ARGC_ITEM (fchmod),
#endif 

#if defined(__NR_fchown)
    SYSCALL_ARGC_ITEM (fchown),
#endif 

#if defined(__NR_getpriority)
    SYSCALL_ARGC_ITEM (getpriority),
#endif 

#if defined(__NR_setpriority)
    SYSCALL_ARGC_ITEM (setpriority),
#endif 

#if defined(__NR_profil)
    SYSCALL_ARGC_ITEM (profil),
#endif 

#if defined(__NR_statfs)
    SYSCALL_ARGC_ITEM (statfs),
#endif 

#if defined(__NR_fstatfs)
    SYSCALL_ARGC_ITEM (fstatfs),
#endif 

#if defined(__NR_ioperm)
    SYSCALL_ARGC_ITEM (ioperm),
#endif 

#if defined(__NR_socketcall)
    SYSCALL_ARGC_ITEM (socketcall),
#endif 

#if defined(__NR_syslog)
    SYSCALL_ARGC_ITEM (syslog),
#endif 

#if defined(__NR_setitimer)
    SYSCALL_ARGC_ITEM (setitimer),
#endif 

#if defined(__NR_getitimer)
    SYSCALL_ARGC_ITEM (getitimer),
#endif 

#if defined(__NR_stat)
    SYSCALL_ARGC_ITEM (stat),
#endif 

#if defined(__NR_lstat)
    SYSCALL_ARGC_ITEM (lstat),
#endif 

#if defined(__NR_fstat)
    SYSCALL_ARGC_ITEM (fstat),
#endif 

#if defined(__NR_olduname)
    SYSCALL_ARGC_ITEM (olduname),
#endif 

#if defined(__NR_iopl)
    SYSCALL_ARGC_ITEM (iopl),
#endif 

#if defined(__NR_vhangup)
    SYSCALL_ARGC_ITEM (vhangup),
#endif 

#if defined(__NR_idle)
    SYSCALL_ARGC_ITEM (idle),
#endif 

#if defined(__NR_vm86old)
    SYSCALL_ARGC_ITEM (vm86old),
#endif 

#if defined(__NR_wait4)
    SYSCALL_ARGC_ITEM (wait4),
#endif 

#if defined(__NR_swapoff)
    SYSCALL_ARGC_ITEM (swapoff),
#endif 

#if defined(__NR_sysinfo)
    SYSCALL_ARGC_ITEM (sysinfo),
#endif 

#if defined(__NR_ipc)
    SYSCALL_ARGC_ITEM (ipc),
#endif 

#if defined(__NR_fsync)
    SYSCALL_ARGC_ITEM (fsync),
#endif 

#if defined(__NR_sigreturn)
    SYSCALL_ARGC_ITEM (sigreturn),
#endif 

#if defined(__NR_clone)
    SYSCALL_ARGC_ITEM (clone),
#endif 

#if defined(__NR_setdomainname)
    SYSCALL_ARGC_ITEM (setdomainname),
#endif 

#if defined(__NR_uname)
    SYSCALL_ARGC_ITEM (uname),
#endif 

#if defined(__NR_modify_ldt)
    SYSCALL_ARGC_ITEM (modify_ldt),
#endif 

#if defined(__NR_adjtimex)
    SYSCALL_ARGC_ITEM (adjtimex),
#endif 

#if defined(__NR_mprotect)
    SYSCALL_ARGC_ITEM (mprotect),
#endif 

#if defined(__NR_sigprocmask)
    SYSCALL_ARGC_ITEM (sigprocmask),
#endif 

#if defined(__NR_quotactl)
    SYSCALL_ARGC_ITEM (quotactl),
#endif 

#if defined(__NR_getpgid)
    SYSCALL_ARGC_ITEM (getpgid),
#endif 

#if defined(__NR_fchdir)
    SYSCALL_ARGC_ITEM (fchdir),
#endif 

#if defined(__NR_bdflush)
    SYSCALL_ARGC_ITEM (bdflush),
#endif 

#if defined(__NR_sysfs)
    SYSCALL_ARGC_ITEM (sysfs),
#endif 

#if defined(__NR_personality)
    SYSCALL_ARGC_ITEM (personality),
#endif 

#if defined(__NR_afs_syscall)
    SYSCALL_ARGC_ITEM (afs_syscall),
#endif 

#if defined(__NR_setfsuid)
    SYSCALL_ARGC_ITEM (setfsuid),
#endif 

#if defined(__NR_setfsgid)
    SYSCALL_ARGC_ITEM (setfsgid),
#endif 

#if defined(__NR__llseek)
    SYSCALL_ARGC_ITEM (_llseek),
#endif 

#if defined(__NR_getdents)
    SYSCALL_ARGC_ITEM (getdents),
#endif 

#if defined(__NR__newselect)
    SYSCALL_ARGC_ITEM (_newselect),
#endif 

#if defined(__NR_flock)
    SYSCALL_ARGC_ITEM (flock),
#endif 

#if defined(__NR_msync)
    SYSCALL_ARGC_ITEM (msync),
#endif 

#if defined(__NR_readv)
    SYSCALL_ARGC_ITEM (readv),
#endif 

#if defined(__NR_writev)
    SYSCALL_ARGC_ITEM (writev),
#endif 

#if defined(__NR_getsid)
    SYSCALL_ARGC_ITEM (getsid),
#endif 

#if defined(__NR_fdatasync)
    SYSCALL_ARGC_ITEM (fdatasync),
#endif 

#if defined(__NR__sysctl)
    SYSCALL_ARGC_ITEM (_sysctl),
#endif 

#if defined(__NR_mlock)
    SYSCALL_ARGC_ITEM (mlock),
#endif 

#if defined(__NR_munlock)
    SYSCALL_ARGC_ITEM (munlock),
#endif 

#if defined(__NR_mlockall)
    SYSCALL_ARGC_ITEM (mlockall),
#endif 

#if defined(__NR_munlockall)
    SYSCALL_ARGC_ITEM (munlockall),
#endif 

#if defined(__NR_sched_setparam)
    SYSCALL_ARGC_ITEM (sched_setparam),
#endif 

#if defined(__NR_sched_getparam)
    SYSCALL_ARGC_ITEM (sched_getparam),
#endif 

#if defined(__NR_sched_setscheduler)
    SYSCALL_ARGC_ITEM (sched_setscheduler),
#endif 

#if defined(__NR_sched_getscheduler)
    SYSCALL_ARGC_ITEM (sched_getscheduler),
#endif 

#if defined(__NR_sched_yield)
    SYSCALL_ARGC_ITEM (sched_yield),
#endif 

#if defined(__NR_sched_get_priority_max)
    SYSCALL_ARGC_ITEM (sched_get_priority_max),
#endif 

#if defined(__NR_sched_get_priority_min)
    SYSCALL_ARGC_ITEM (sched_get_priority_min),
#endif 

#if defined(__NR_sched_rr_get_interval)
    SYSCALL_ARGC_ITEM (sched_rr_get_interval),
#endif 

#if defined(__NR_nanosleep)
    SYSCALL_ARGC_ITEM (nanosleep),
#endif 

#if defined(__NR_mremap)
    SYSCALL_ARGC_ITEM (mremap),
#endif 

#if defined(__NR_setresuid)
    SYSCALL_ARGC_ITEM (setresuid),
#endif 

#if defined(__NR_getresuid)
    SYSCALL_ARGC_ITEM (getresuid),
#endif 

#if defined(__NR_vm86)
    SYSCALL_ARGC_ITEM (vm86),
#endif 

#if defined(__NR_query_module)
    SYSCALL_ARGC_ITEM (query_module),
#endif 

#if defined(__NR_poll)
    SYSCALL_ARGC_ITEM (poll),
#endif 

#if defined(__NR_nfsservctl)
    SYSCALL_ARGC_ITEM (nfsservctl),
#endif 

#if defined(__NR_setresgid)
    SYSCALL_ARGC_ITEM (setresgid),
#endif 

#if defined(__NR_getresgid)
    SYSCALL_ARGC_ITEM (getresgid),
#endif 

#if defined(__NR_prctl)
    SYSCALL_ARGC_ITEM (prctl),
#endif 

#if defined(__NR_rt_sigreturn)
    SYSCALL_ARGC_ITEM (rt_sigreturn),
#endif 

#if defined(__NR_rt_sigaction)
    SYSCALL_ARGC_ITEM (rt_sigaction),
#endif 

#if defined(__NR_rt_sigprocmask)
    SYSCALL_ARGC_ITEM (rt_sigprocmask),
#endif 

#if defined(__NR_rt_sigpending)
    SYSCALL_ARGC_ITEM (rt_sigpending),
#endif 

#if defined(__NR_rt_sigtimedwait)
    SYSCALL_ARGC_ITEM (rt_sigtimedwait),
#endif 

#if defined(__NR_rt_sigqueueinfo)
    SYSCALL_ARGC_ITEM (rt_sigqueueinfo),
#endif 

#if defined(__NR_rt_sigsuspend)
    SYSCALL_ARGC_ITEM (rt_sigsuspend),
#endif 

#if defined(__NR_pread64)
    SYSCALL_ARGC_ITEM (pread64),
#endif 

#if defined(__NR_pwrite64)
    SYSCALL_ARGC_ITEM (pwrite64),
#endif 

#if defined(__NR_chown)
    SYSCALL_ARGC_ITEM (chown),
#endif 

#if defined(__NR_getcwd)
    SYSCALL_ARGC_ITEM (getcwd),
#endif 

#if defined(__NR_capget)
    SYSCALL_ARGC_ITEM (capget),
#endif 

#if defined(__NR_capset)
    SYSCALL_ARGC_ITEM (capset),
#endif 

#if defined(__NR_sigaltstack)
    SYSCALL_ARGC_ITEM (sigaltstack),
#endif 

#if defined(__NR_sendfile)
    SYSCALL_ARGC_ITEM (sendfile),
#endif 

#if defined(__NR_getpmsg)
    SYSCALL_ARGC_ITEM (getpmsg),
#endif 

#if defined(__NR_putpmsg)
    SYSCALL_ARGC_ITEM (putpmsg),
#endif 

#if defined(__NR_vfork)
    SYSCALL_ARGC_ITEM (vfork),
#endif 

#if defined(__NR_ugetrlimit)
    SYSCALL_ARGC_ITEM (ugetrlimit),
#endif 

#if defined(__NR_mmap2)
    SYSCALL_ARGC_ITEM (mmap2),
#endif 

#if defined(__NR_truncate64)
    SYSCALL_ARGC_ITEM (truncate64),
#endif 

#if defined(__NR_ftruncate64)
    SYSCALL_ARGC_ITEM (ftruncate64),
#endif 

#if defined(__NR_stat64)
    SYSCALL_ARGC_ITEM (stat64),
#endif 

#if defined(__NR_lstat64)
    SYSCALL_ARGC_ITEM (lstat64),
#endif 

#if defined(__NR_fstat64)
    SYSCALL_ARGC_ITEM (fstat64),
#endif 

#if defined(__NR_lchown32)
    SYSCALL_ARGC_ITEM (lchown32),
#endif 

#if defined(__NR_getuid32)
    SYSCALL_ARGC_ITEM (getuid32),
#endif 

#if defined(__NR_getgid32)
    SYSCALL_ARGC_ITEM (getgid32),
#endif 

#if defined(__NR_geteuid32)
    SYSCALL_ARGC_ITEM (geteuid32),
#endif 

#if defined(__NR_getegid32)
    SYSCALL_ARGC_ITEM (getegid32),
#endif 

#if defined(__NR_setreuid32)
    SYSCALL_ARGC_ITEM (setreuid32),
#endif 

#if defined(__NR_setregid32)
    SYSCALL_ARGC_ITEM (setregid32),
#endif 

#if defined(__NR_getgroups32)
    SYSCALL_ARGC_ITEM (getgroups32),
#endif 

#if defined(__NR_setgroups32)
    SYSCALL_ARGC_ITEM (setgroups32),
#endif 

#if defined(__NR_fchown32)
    SYSCALL_ARGC_ITEM (fchown32),
#endif 

#if defined(__NR_setresuid32)
    SYSCALL_ARGC_ITEM (setresuid32),
#endif 

#if defined(__NR_getresuid32)
    SYSCALL_ARGC_ITEM (getresuid32),
#endif 

#if defined(__NR_setresgid32)
    SYSCALL_ARGC_ITEM (setresgid32),
#endif 

#if defined(__NR_getresgid32)
    SYSCALL_ARGC_ITEM (getresgid32),
#endif 

#if defined(__NR_chown32)
    SYSCALL_ARGC_ITEM (chown32),
#endif 

#if defined(__NR_setuid32)
    SYSCALL_ARGC_ITEM (setuid32),
#endif 

#if defined(__NR_setgid32)
    SYSCALL_ARGC_ITEM (setgid32),
#endif 

#if defined(__NR_setfsuid32)
    SYSCALL_ARGC_ITEM (setfsuid32),
#endif 

#if defined(__NR_setfsgid32)
    SYSCALL_ARGC_ITEM (setfsgid32),
#endif 

#if defined(__NR_pivot_root)
    SYSCALL_ARGC_ITEM (pivot_root),
#endif 

#if defined(__NR_mincore)
    SYSCALL_ARGC_ITEM (mincore),
#endif 

#if defined(__NR_madvise)
    SYSCALL_ARGC_ITEM (madvise),
#endif 

#if defined(__NR_getdents64)
    SYSCALL_ARGC_ITEM (getdents64),
#endif 

#if defined(__NR_fcntl64)
    SYSCALL_ARGC_ITEM (fcntl64),
#endif 

#if defined(__NR_gettid)
    SYSCALL_ARGC_ITEM (gettid),
#endif 

#if defined(__NR_readahead)
    SYSCALL_ARGC_ITEM (readahead),
#endif 

#if defined(__NR_setxattr)
    SYSCALL_ARGC_ITEM (setxattr),
#endif 

#if defined(__NR_lsetxattr)
    SYSCALL_ARGC_ITEM (lsetxattr),
#endif 

#if defined(__NR_fsetxattr)
    SYSCALL_ARGC_ITEM (fsetxattr),
#endif 

#if defined(__NR_getxattr)
    SYSCALL_ARGC_ITEM (getxattr),
#endif 

#if defined(__NR_lgetxattr)
    SYSCALL_ARGC_ITEM (lgetxattr),
#endif 

#if defined(__NR_fgetxattr)
    SYSCALL_ARGC_ITEM (fgetxattr),
#endif 

#if defined(__NR_listxattr)
    SYSCALL_ARGC_ITEM (listxattr),
#endif 

#if defined(__NR_llistxattr)
    SYSCALL_ARGC_ITEM (llistxattr),
#endif 

#if defined(__NR_flistxattr)
    SYSCALL_ARGC_ITEM (flistxattr),
#endif 

#if defined(__NR_removexattr)
    SYSCALL_ARGC_ITEM (removexattr),
#endif 

#if defined(__NR_lremovexattr)
    SYSCALL_ARGC_ITEM (lremovexattr),
#endif 

#if defined(__NR_fremovexattr)
    SYSCALL_ARGC_ITEM (fremovexattr),
#endif 

#if defined(__NR_tkill)
    SYSCALL_ARGC_ITEM (tkill),
#endif 

#if defined(__NR_sendfile64)
    SYSCALL_ARGC_ITEM (sendfile64),
#endif 

#if defined(__NR_futex)
    SYSCALL_ARGC_ITEM (futex),
#endif 

#if defined(__NR_sched_setaffinity)
    SYSCALL_ARGC_ITEM (sched_setaffinity),
#endif 

#if defined(__NR_sched_getaffinity)
    SYSCALL_ARGC_ITEM (sched_getaffinity),
#endif 

#if defined(__NR_set_thread_area)
    SYSCALL_ARGC_ITEM (set_thread_area),
#endif 

#if defined(__NR_get_thread_area)
    SYSCALL_ARGC_ITEM (get_thread_area),
#endif 

#if defined(__NR_io_setup)
    SYSCALL_ARGC_ITEM (io_setup),
#endif 

#if defined(__NR_io_destroy)
    SYSCALL_ARGC_ITEM (io_destroy),
#endif 

#if defined(__NR_io_getevents)
    SYSCALL_ARGC_ITEM (io_getevents),
#endif 

#if defined(__NR_io_submit)
    SYSCALL_ARGC_ITEM (io_submit),
#endif 

#if defined(__NR_io_cancel)
    SYSCALL_ARGC_ITEM (io_cancel),
#endif 

#if defined(__NR_fadvise64)
    SYSCALL_ARGC_ITEM (fadvise64),
#endif 

#if defined(__NR_exit_group)
    SYSCALL_ARGC_ITEM (exit_group),
#endif 

#if defined(__NR_lookup_dcookie)
    SYSCALL_ARGC_ITEM (lookup_dcookie),
#endif 

#if defined(__NR_epoll_create)
    SYSCALL_ARGC_ITEM (epoll_create),
#endif 

#if defined(__NR_epoll_ctl)
    SYSCALL_ARGC_ITEM (epoll_ctl),
#endif 

#if defined(__NR_epoll_wait)
    SYSCALL_ARGC_ITEM (epoll_wait),
#endif 

#if defined(__NR_remap_file_pages)
    SYSCALL_ARGC_ITEM (remap_file_pages),
#endif 

#if defined(__NR_set_tid_address)
    SYSCALL_ARGC_ITEM (set_tid_address),
#endif 

#if defined(__NR_timer_create)
    SYSCALL_ARGC_ITEM (timer_create),
#endif 

#if defined(__NR_timer_settime32)
    SYSCALL_ARGC_ITEM (timer_settime32),
#endif 

#if defined(__NR_timer_settime)
    SYSCALL_ARGC_ITEM (timer_settime),
#endif 

#if defined(__NR_timer_gettime32)
    SYSCALL_ARGC_ITEM (timer_gettime32),
#endif 

#if defined(__NR_timer_gettime)
    SYSCALL_ARGC_ITEM (timer_gettime),
#endif 

#if defined(__NR_timer_getoverrun)
    SYSCALL_ARGC_ITEM (timer_getoverrun),
#endif 

#if defined(__NR_timer_delete)
    SYSCALL_ARGC_ITEM (timer_delete),
#endif 

#if defined(__NR_clock_settime32)
    SYSCALL_ARGC_ITEM (clock_settime32),
#endif 

#if defined(__NR_clock_gettime32)
    SYSCALL_ARGC_ITEM (clock_gettime32),
#endif 

#if defined(__NR_clock_gettime)
    SYSCALL_ARGC_ITEM (clock_gettime),
#endif

#if defined(__NR_clock_getres_time32)
    SYSCALL_ARGC_ITEM (clock_getres_time32),
#endif

#if defined(__NR_clock_nanosleep_time32)
    SYSCALL_ARGC_ITEM (clock_nanosleep_time32),
#endif

#if defined(__NR_statfs64)
    SYSCALL_ARGC_ITEM (statfs64),
#endif 

#if defined(__NR_fstatfs64)
    SYSCALL_ARGC_ITEM (fstatfs64),
#endif 

#if defined(__NR_tgkill)
    SYSCALL_ARGC_ITEM (tgkill),
#endif 

#if defined(__NR_utimes)
    SYSCALL_ARGC_ITEM (utimes),
#endif 

#if defined(__NR_fadvise64_64)
    SYSCALL_ARGC_ITEM (fadvise64_64),
#endif 

#if defined(__NR_vserver)
    SYSCALL_ARGC_ITEM (vserver),
#endif 

#if defined(__NR_mbind)
    SYSCALL_ARGC_ITEM (mbind),
#endif 

#if defined(__NR_get_mempolicy)
    SYSCALL_ARGC_ITEM (get_mempolicy),
#endif 

#if defined(__NR_set_mempolicy)
    SYSCALL_ARGC_ITEM (set_mempolicy),
#endif 

#if defined(__NR_mq_open)
    SYSCALL_ARGC_ITEM (mq_open),
#endif 

#if defined(__NR_mq_unlink)
    SYSCALL_ARGC_ITEM (mq_unlink),
#endif 

#if defined(__NR_mq_timedsend)
    SYSCALL_ARGC_ITEM (mq_timedsend),
#endif 

#if defined(__NR_mq_timedreceive)
    SYSCALL_ARGC_ITEM (mq_timedreceive),
#endif 

#if defined(__NR_mq_notify)
    SYSCALL_ARGC_ITEM (mq_notify),
#endif 

#if defined(__NR_mq_getsetattr)
    SYSCALL_ARGC_ITEM (mq_getsetattr),
#endif 

#if defined(__NR_kexec_load)
    SYSCALL_ARGC_ITEM (kexec_load),
#endif 

#if defined(__NR_waitid)
    SYSCALL_ARGC_ITEM (waitid),
#endif 

#if defined(__NR_add_key)
    SYSCALL_ARGC_ITEM (add_key),
#endif 

#if defined(__NR_request_key)
    SYSCALL_ARGC_ITEM (request_key),
#endif 

#if defined(__NR_keyctl)
    SYSCALL_ARGC_ITEM (keyctl),
#endif 

#if defined(__NR_ioprio_set)
    SYSCALL_ARGC_ITEM (ioprio_set),
#endif 

#if defined(__NR_ioprio_get)
    SYSCALL_ARGC_ITEM (ioprio_get),
#endif 

#if defined(__NR_inotify_init)
    SYSCALL_ARGC_ITEM (inotify_init),
#endif 

#if defined(__NR_inotify_add_watch)
    SYSCALL_ARGC_ITEM (inotify_add_watch),
#endif 

#if defined(__NR_inotify_rm_watch)
    SYSCALL_ARGC_ITEM (inotify_rm_watch),
#endif 

#if defined(__NR_migrate_pages)
    SYSCALL_ARGC_ITEM (migrate_pages),
#endif 

#if defined(__NR_openat)
    SYSCALL_ARGC_ITEM (openat),
#endif 

#if defined(__NR_mkdirat)
    SYSCALL_ARGC_ITEM (mkdirat),
#endif 

#if defined(__NR_mknodat)
    SYSCALL_ARGC_ITEM (mknodat),
#endif 

#if defined(__NR_fchownat)
    SYSCALL_ARGC_ITEM (fchownat),
#endif 

#if defined(__NR_futimesat)
    SYSCALL_ARGC_ITEM (futimesat),
#endif 

#if defined(__NR_fstatat64)
    SYSCALL_ARGC_ITEM (fstatat64),
#endif 

#if defined(__NR_newfstatat)
    SYSCALL_ARGC_ITEM (newfstatat),
#endif 

#if defined(__NR_unlinkat)
    SYSCALL_ARGC_ITEM (unlinkat),
#endif 

#if defined(__NR_renameat)
    SYSCALL_ARGC_ITEM (renameat),
#endif 

#if defined(__NR_linkat)
    SYSCALL_ARGC_ITEM (linkat),
#endif 

#if defined(__NR_symlinkat)
    SYSCALL_ARGC_ITEM (symlinkat),
#endif 

#if defined(__NR_readlinkat)
    SYSCALL_ARGC_ITEM (readlinkat),
#endif 

#if defined(__NR_fchmodat)
    SYSCALL_ARGC_ITEM (fchmodat),
#endif 

#if defined(__NR_faccessat)
    SYSCALL_ARGC_ITEM (faccessat),
#endif 

#if defined(__NR_pselect6)
    SYSCALL_ARGC_ITEM (pselect6),
#endif 

#if defined(__NR_ppoll)
    SYSCALL_ARGC_ITEM (ppoll),
#endif 

#if defined(__NR_unshare)
    SYSCALL_ARGC_ITEM (unshare),
#endif 

#if defined(__NR_set_robust_list)
    SYSCALL_ARGC_ITEM (set_robust_list),
#endif 

#if defined(__NR_get_robust_list)
    SYSCALL_ARGC_ITEM (get_robust_list),
#endif 

#if defined(__NR_splice)
    SYSCALL_ARGC_ITEM (splice),
#endif 

#if defined(__NR_sync_file_range)
    SYSCALL_ARGC_ITEM (sync_file_range),
#endif 

#if defined(__NR_tee)
    SYSCALL_ARGC_ITEM (tee),
#endif 

#if defined(__NR_vmsplice)
    SYSCALL_ARGC_ITEM (vmsplice),
#endif 

#if defined(__NR_move_pages)
    SYSCALL_ARGC_ITEM (move_pages),
#endif 

#if defined(__NR_getcpu)
    SYSCALL_ARGC_ITEM (getcpu),
#endif 

#if defined(__NR_epoll_pwait)
    SYSCALL_ARGC_ITEM (epoll_pwait),
#endif 

#if defined(__NR_utimensat)
    SYSCALL_ARGC_ITEM (utimensat),
#endif 

#if defined(__NR_signalfd)
    SYSCALL_ARGC_ITEM (signalfd),
#endif 

#if defined(__NR_timerfd_create)
    SYSCALL_ARGC_ITEM (timerfd_create),
#endif 

#if defined(__NR_eventfd)
    SYSCALL_ARGC_ITEM (eventfd),
#endif 

#if defined(__NR_fallocate)
    SYSCALL_ARGC_ITEM (fallocate),
#endif 

#if defined(__NR_timerfd_settime32)
    SYSCALL_ARGC_ITEM (timerfd_settime32),
#endif 

#if defined(__NR_timerfd_gettime32)
    SYSCALL_ARGC_ITEM (timerfd_gettime32),
#endif 

#if defined(__NR_signalfd4)
    SYSCALL_ARGC_ITEM (signalfd4),
#endif 

#if defined(__NR_eventfd2)
    SYSCALL_ARGC_ITEM (eventfd2),
#endif 

#if defined(__NR_epoll_create1)
    SYSCALL_ARGC_ITEM (epoll_create1),
#endif 

#if defined(__NR_dup3)
    SYSCALL_ARGC_ITEM (dup3),
#endif 

#if defined(__NR_pipe2)
    SYSCALL_ARGC_ITEM (pipe2),
#endif 

#if defined(__NR_inotify_init1)
    SYSCALL_ARGC_ITEM (inotify_init1),
#endif 

#if defined(__NR_preadv)
    SYSCALL_ARGC_ITEM (preadv),
#endif 

#if defined(__NR_pwritev)
    SYSCALL_ARGC_ITEM (pwritev),
#endif 

#if defined(__NR_rt_tgsigqueueinfo)
    SYSCALL_ARGC_ITEM (rt_tgsigqueueinfo),
#endif 

#if defined(__NR_perf_event_open)
    SYSCALL_ARGC_ITEM (perf_event_open),
#endif 

#if defined(__NR_recvmmsg)
    SYSCALL_ARGC_ITEM (recvmmsg),
#endif 

#if defined(__NR_fanotify_init)
    SYSCALL_ARGC_ITEM (fanotify_init),
#endif 

#if defined(__NR_fanotify_mark)
    SYSCALL_ARGC_ITEM (fanotify_mark),
#endif 

#if defined(__NR_prlimit64)
    SYSCALL_ARGC_ITEM (prlimit64),
#endif 

#if defined(__NR_name_to_handle_at)
    SYSCALL_ARGC_ITEM (name_to_handle_at),
#endif 

#if defined(__NR_open_by_handle_at)
    SYSCALL_ARGC_ITEM (open_by_handle_at),
#endif 

#if defined(__NR_clock_adjtime)
    SYSCALL_ARGC_ITEM (clock_adjtime),
#endif 

#if defined(__NR_syncfs)
    SYSCALL_ARGC_ITEM (syncfs),
#endif 

#if defined(__NR_sendmmsg)
    SYSCALL_ARGC_ITEM (sendmmsg),
#endif 

#if defined(__NR_setns)
    SYSCALL_ARGC_ITEM (setns),
#endif 

#if defined(__NR_process_vm_readv)
    SYSCALL_ARGC_ITEM (process_vm_readv),
#endif 

#if defined(__NR_process_vm_writev)
    SYSCALL_ARGC_ITEM (process_vm_writev),
#endif 

#if defined(__NR_kcmp)
    SYSCALL_ARGC_ITEM (kcmp),
#endif 

#if defined(__NR_sched_setattr)
    SYSCALL_ARGC_ITEM (sched_setattr),
#endif 

#if defined(__NR_sched_getattr)
    SYSCALL_ARGC_ITEM (sched_getattr),
#endif 

#if defined(__NR_renameat2)
    SYSCALL_ARGC_ITEM (renameat2),
#endif 

#if defined(__NR_seccomp)
    SYSCALL_ARGC_ITEM (seccomp),
#endif 

#if defined(__NR_getrandom)
    SYSCALL_ARGC_ITEM (getrandom),
#endif 

#if defined(__NR_memfd_create)
    SYSCALL_ARGC_ITEM (memfd_create),
#endif 

#if defined(__NR_bpf)
    SYSCALL_ARGC_ITEM (bpf),
#endif 

#if defined(__NR_execveat)
    SYSCALL_ARGC_ITEM (execveat),
#endif 

#if defined(__NR_socket)
    SYSCALL_ARGC_ITEM (socket),
#endif 

#if defined(__NR_socketpair)
    SYSCALL_ARGC_ITEM (socketpair),
#endif 

#if defined(__NR_bind)
    SYSCALL_ARGC_ITEM (bind),
#endif 

#if defined(__NR_connect)
    SYSCALL_ARGC_ITEM (connect),
#endif 

#if defined(__NR_listen)
    SYSCALL_ARGC_ITEM (listen),
#endif 

#if defined(__NR_accept)
    SYSCALL_ARGC_ITEM (accept),
#endif 

#if defined(__NR_accept4)
    SYSCALL_ARGC_ITEM (accept4),
#endif 

#if defined(__NR_getsockopt)
    SYSCALL_ARGC_ITEM (getsockopt),
#endif 

#if defined(__NR_setsockopt)
    SYSCALL_ARGC_ITEM (setsockopt),
#endif 

#if defined(__NR_getsockname)
    SYSCALL_ARGC_ITEM (getsockname),
#endif 

#if defined(__NR_getpeername)
    SYSCALL_ARGC_ITEM (getpeername),
#endif 

#if defined(__NR_sendto)
    SYSCALL_ARGC_ITEM (sendto),
#endif 

#if defined(__NR_sendmsg)
    SYSCALL_ARGC_ITEM (sendmsg),
#endif 

#if defined(__NR_recvfrom)
    SYSCALL_ARGC_ITEM (recvfrom),
#endif 

#if defined(__NR_recvmsg)
    SYSCALL_ARGC_ITEM (recvmsg),
#endif 

#if defined(__NR_shutdown)
    SYSCALL_ARGC_ITEM (shutdown),
#endif 

#if defined(__NR_userfaultfd)
    SYSCALL_ARGC_ITEM (userfaultfd),
#endif 

#if defined(__NR_membarrier)
    SYSCALL_ARGC_ITEM (membarrier),
#endif 

#if defined(__NR_mlock2)
    SYSCALL_ARGC_ITEM (mlock2),
#endif 

#if defined(__NR_copy_file_range)
    SYSCALL_ARGC_ITEM (copy_file_range),
#endif 

#if defined(__NR_preadv2)
    SYSCALL_ARGC_ITEM (preadv2),
#endif 

#if defined(__NR_pwritev2)
    SYSCALL_ARGC_ITEM (pwritev2),
#endif 

#if defined(__NR_pkey_mprotect)
    SYSCALL_ARGC_ITEM (pkey_mprotect),
#endif 

#if defined(__NR_pkey_alloc)
    SYSCALL_ARGC_ITEM (pkey_alloc),
#endif 

#if defined(__NR_pkey_free)
    SYSCALL_ARGC_ITEM (pkey_free),
#endif 

#if defined(__NR_statx)
    SYSCALL_ARGC_ITEM (statx),
#endif 

#if defined(__NR_arch_prctl)
    SYSCALL_ARGC_ITEM (arch_prctl),
#endif 

#if defined(__NR_io_pgetevents)
    SYSCALL_ARGC_ITEM (io_pgetevents),
#endif 

#if defined(__NR_rseq)
    SYSCALL_ARGC_ITEM (rseq),
#endif 

#if defined(__NR_semget)
    SYSCALL_ARGC_ITEM (semget),
#endif 

#if defined(__NR_semctl)
    SYSCALL_ARGC_ITEM (semctl),
#endif 

#if defined(__NR_shmget)
    SYSCALL_ARGC_ITEM (shmget),
#endif 

#if defined(__NR_shmctl)
    SYSCALL_ARGC_ITEM (shmctl),
#endif 

#if defined(__NR_shmat)
    SYSCALL_ARGC_ITEM (shmat),
#endif 

#if defined(__NR_shmdt)
    SYSCALL_ARGC_ITEM (shmdt),
#endif 

#if defined(__NR_msgget)
    SYSCALL_ARGC_ITEM (msgget),
#endif 

#if defined(__NR_msgsnd)
    SYSCALL_ARGC_ITEM (msgsnd),
#endif 

#if defined(__NR_msgrcv)
    SYSCALL_ARGC_ITEM (msgrcv),
#endif 

#if defined(__NR_msgctl)
    SYSCALL_ARGC_ITEM (msgctl),
#endif 

#if defined(__NR_clock_gettime64)
    SYSCALL_ARGC_ITEM (clock_gettime64),
#endif 

#if defined(__NR_clock_settime64)
    SYSCALL_ARGC_ITEM (clock_settime64),
#endif 

#if defined(__NR_clock_adjtime64)
    SYSCALL_ARGC_ITEM (clock_adjtime64),
#endif 

#if defined(__NR_clock_getres)
    SYSCALL_ARGC_ITEM (clock_getres),
#endif 

#if defined(__NR_clock_getres_time64)
    SYSCALL_ARGC_ITEM (clock_getres_time64),
#endif 

#if defined(__NR_clock_nanosleep)
    SYSCALL_ARGC_ITEM (clock_nanosleep),
#endif 

#if defined(__NR_clock_nanosleep_time64)
    SYSCALL_ARGC_ITEM (clock_nanosleep_time64),
#endif 

#if defined(__NR_timer_gettime64)
    SYSCALL_ARGC_ITEM (timer_gettime64),
#endif 

#if defined(__NR_timer_settime64)
    SYSCALL_ARGC_ITEM (timer_settime64),
#endif 

#if defined(__NR_timerfd_gettime64)
    SYSCALL_ARGC_ITEM (timerfd_gettime64),
#endif 

#if defined(__NR_timerfd_settime64)
    SYSCALL_ARGC_ITEM (timerfd_settime64),
#endif 

#if defined(__NR_utimensat_time64)
    SYSCALL_ARGC_ITEM (utimensat_time64),
#endif 

#if defined(__NR_pselect6_time64)
    SYSCALL_ARGC_ITEM (pselect6_time64),
#endif 

#if defined(__NR_ppoll_time64)
    SYSCALL_ARGC_ITEM (ppoll_time64),
#endif 

#if defined(__NR_io_pgetevents_time64)
    SYSCALL_ARGC_ITEM (io_pgetevents_time64),
#endif 

#if defined(__NR_recvmmsg_time64)
    SYSCALL_ARGC_ITEM (recvmmsg_time64),
#endif 

#if defined(__NR_mq_timedsend_time64)
    SYSCALL_ARGC_ITEM (mq_timedsend_time64),
#endif 

#if defined(__NR_mq_timedreceive_time64)
    SYSCALL_ARGC_ITEM (mq_timedreceive_time64),
#endif 

#if defined(__NR_semtimedop_time64)
    SYSCALL_ARGC_ITEM (semtimedop_time64),
#endif 

#if defined(__NR_rt_sigtimedwait_time64)
    SYSCALL_ARGC_ITEM (rt_sigtimedwait_time64),
#endif 

#if defined(__NR_futex_time64)
    SYSCALL_ARGC_ITEM (futex_time64),
#endif 

#if defined(__NR_sched_rr_get_interval_time64)
    SYSCALL_ARGC_ITEM (sched_rr_get_interval_time64),
#endif 

#if defined(__NR_pidfd_send_signal)
    SYSCALL_ARGC_ITEM (pidfd_send_signal),
#endif 

#if defined(__NR_io_uring_setup)
    SYSCALL_ARGC_ITEM (io_uring_setup),
#endif 

#if defined(__NR_io_uring_enter)
    SYSCALL_ARGC_ITEM (io_uring_enter),
#endif 

#if defined(__NR_io_uring_register)
    SYSCALL_ARGC_ITEM (io_uring_register),
#endif 

#if defined(__NR_open_tree)
    SYSCALL_ARGC_ITEM (open_tree),
#endif 

#if defined(__NR_move_mount)
    SYSCALL_ARGC_ITEM (move_mount),
#endif 

#if defined(__NR_fsopen)
    SYSCALL_ARGC_ITEM (fsopen),
#endif 

#if defined(__NR_fsconfig)
    SYSCALL_ARGC_ITEM (fsconfig),
#endif 

#if defined(__NR_fsmount)
    SYSCALL_ARGC_ITEM (fsmount),
#endif 

#if defined(__NR_fspick)
    SYSCALL_ARGC_ITEM (fspick),
#endif 

#if defined(__NR_pidfd_open)
    SYSCALL_ARGC_ITEM (pidfd_open),
#endif 

#if defined(__NR_clone3)
    SYSCALL_ARGC_ITEM (clone3),
#endif 

#if defined(__NR_close_range)
    SYSCALL_ARGC_ITEM (close_range),
#endif 

#if defined(__NR_openat2)
    SYSCALL_ARGC_ITEM (openat2),
#endif 

#if defined(__NR_pidfd_getfd)
    SYSCALL_ARGC_ITEM (pidfd_getfd),
#endif 

#if defined(__NR_faccessat2)
    SYSCALL_ARGC_ITEM (faccessat2),
#endif 

#if defined(__NR_process_madvise)
    SYSCALL_ARGC_ITEM (process_madvise),
#endif 

#if defined(__NR_epoll_pwait2)
    SYSCALL_ARGC_ITEM (epoll_pwait2),
#endif 

#if defined(__NR_mount_setattr)
    SYSCALL_ARGC_ITEM (mount_setattr),
#endif 

#if defined(__NR_landlock_create_ruleset)
    SYSCALL_ARGC_ITEM (landlock_create_ruleset),
#endif 

#if defined(__NR_landlock_add_rule)
    SYSCALL_ARGC_ITEM (landlock_add_rule),
#endif 

    SYSCALL_ARGC_ITEM (landlock_restrict_self)
};

int syscall_get_argc_count(long syscall_num)
{
    return syscall_argc_table[syscall_num];
}

static const char *syscall_name_table[] = {
#if defined(__NR_restart_syscall)
    SYSCALL_NAME_ITEM (restart_syscall),
#endif 

#if defined(__NR_exit)
    SYSCALL_NAME_ITEM (exit),
#endif 

#if defined(__NR_fork)
    SYSCALL_NAME_ITEM (fork),
#endif 

#if defined(__NR_read)
    SYSCALL_NAME_ITEM (read),
#endif 

#if defined(__NR_write)
    SYSCALL_NAME_ITEM (write),
#endif 

#if defined(__NR_open)
    SYSCALL_NAME_ITEM (open),
#endif 

#if defined(__NR_close)
    SYSCALL_NAME_ITEM (close),
#endif 

#if defined(__NR_waitpid)
    SYSCALL_NAME_ITEM (waitpid),
#endif 

#if defined(__NR_creat)
    SYSCALL_NAME_ITEM (creat),
#endif 

#if defined(__NR_link)
    SYSCALL_NAME_ITEM (link),
#endif 

#if defined(__NR_unlink)
    SYSCALL_NAME_ITEM (unlink),
#endif 

#if defined(__NR_execve)
    SYSCALL_NAME_ITEM (execve),
#endif 

#if defined(__NR_chdir)
    SYSCALL_NAME_ITEM (chdir),
#endif 

#if defined(__NR_time)
    SYSCALL_NAME_ITEM (time),
#endif 

#if defined(__NR_mknod)
    SYSCALL_NAME_ITEM (mknod),
#endif 

#if defined(__NR_chmod)
    SYSCALL_NAME_ITEM (chmod),
#endif 

#if defined(__NR_lchown)
    SYSCALL_NAME_ITEM (lchown),
#endif 

#if defined(__NR_break)
    SYSCALL_NAME_ITEM (break),
#endif 

#if defined(__NR_oldstat)
    SYSCALL_NAME_ITEM (oldstat),
#endif 

#if defined(__NR_lseek)
    SYSCALL_NAME_ITEM (lseek),
#endif 

#if defined(__NR_getpid)
    SYSCALL_NAME_ITEM (getpid),
#endif 

#if defined(__NR_mount)
    SYSCALL_NAME_ITEM (mount),
#endif 

#if defined(__NR_umount)
    SYSCALL_NAME_ITEM (umount),
#endif 

#if defined(__NR_setuid)
    SYSCALL_NAME_ITEM (setuid),
#endif 

#if defined(__NR_getuid)
    SYSCALL_NAME_ITEM (getuid),
#endif 

#if defined(__NR_stime)
    SYSCALL_NAME_ITEM (stime),
#endif 

#if defined(__NR_alarm)
    SYSCALL_NAME_ITEM (alarm),
#endif 

#if defined(__NR_oldfstat)
    SYSCALL_NAME_ITEM (oldfstat),
#endif 

#if defined(__NR_pause)
    SYSCALL_NAME_ITEM (pause),
#endif 

#if defined(__NR_utime)
    SYSCALL_NAME_ITEM (utime),
#endif 

#if defined(__NR_stty)
    SYSCALL_NAME_ITEM (stty),
#endif 

#if defined(__NR_gtty)
    SYSCALL_NAME_ITEM (gtty),
#endif 

#if defined(__NR_access)
    SYSCALL_NAME_ITEM (access),
#endif 

#if defined(__NR_nice)
    SYSCALL_NAME_ITEM (nice),
#endif 

#if defined(__NR_ftime)
    SYSCALL_NAME_ITEM (ftime),
#endif 

#if defined(__NR_sync)
    SYSCALL_NAME_ITEM (sync),
#endif 

#if defined(__NR_kill)
    SYSCALL_NAME_ITEM (kill),
#endif 

#if defined(__NR_rename)
    SYSCALL_NAME_ITEM (rename),
#endif 

#if defined(__NR_mkdir)
    SYSCALL_NAME_ITEM (mkdir),
#endif 

#if defined(__NR_rmdir)
    SYSCALL_NAME_ITEM (rmdir),
#endif 

#if defined(__NR_dup)
    SYSCALL_NAME_ITEM (dup),
#endif 

#if defined(__NR_pipe)
    SYSCALL_NAME_ITEM (pipe),
#endif 

#if defined(__NR_times)
    SYSCALL_NAME_ITEM (times),
#endif 

#if defined(__NR_prof)
    SYSCALL_NAME_ITEM (prof),
#endif 

#if defined(__NR_brk)
    SYSCALL_NAME_ITEM (brk),
#endif 

#if defined(__NR_setgid)
    SYSCALL_NAME_ITEM (setgid),
#endif 

#if defined(__NR_getgid)
    SYSCALL_NAME_ITEM (getgid),
#endif 

#if defined(__NR_signal)
    SYSCALL_NAME_ITEM (signal),
#endif 

#if defined(__NR_geteuid)
    SYSCALL_NAME_ITEM (geteuid),
#endif 

#if defined(__NR_getegid)
    SYSCALL_NAME_ITEM (getegid),
#endif 

#if defined(__NR_acct)
    SYSCALL_NAME_ITEM (acct),
#endif 

#if defined(__NR_umount2)
    SYSCALL_NAME_ITEM (umount2),
#endif 

#if defined(__NR_lock)
    SYSCALL_NAME_ITEM (lock),
#endif 

#if defined(__NR_ioctl)
    SYSCALL_NAME_ITEM (ioctl),
#endif 

#if defined(__NR_fcntl)
    SYSCALL_NAME_ITEM (fcntl),
#endif 

#if defined(__NR_mpx)
    SYSCALL_NAME_ITEM (mpx),
#endif 

#if defined(__NR_setpgid)
    SYSCALL_NAME_ITEM (setpgid),
#endif 

#if defined(__NR_ulimit)
    SYSCALL_NAME_ITEM (ulimit),
#endif 

#if defined(__NR_oldolduname)
    SYSCALL_NAME_ITEM (oldolduname),
#endif 

#if defined(__NR_umask)
    SYSCALL_NAME_ITEM (umask),
#endif 

#if defined(__NR_chroot)
    SYSCALL_NAME_ITEM (chroot),
#endif 

#if defined(__NR_ustat)
    SYSCALL_NAME_ITEM (ustat),
#endif 

#if defined(__NR_dup2)
    SYSCALL_NAME_ITEM (dup2),
#endif 

#if defined(__NR_getppid)
    SYSCALL_NAME_ITEM (getppid),
#endif 

#if defined(__NR_getpgrp)
    SYSCALL_NAME_ITEM (getpgrp),
#endif 

#if defined(__NR_setsid)
    SYSCALL_NAME_ITEM (setsid),
#endif 

#if defined(__NR_sigaction)
    SYSCALL_NAME_ITEM (sigaction),
#endif 

#if defined(__NR_sgetmask)
    SYSCALL_NAME_ITEM (sgetmask),
#endif 

#if defined(__NR_ssetmask)
    SYSCALL_NAME_ITEM (ssetmask),
#endif 

#if defined(__NR_setreuid)
    SYSCALL_NAME_ITEM (setreuid),
#endif 

#if defined(__NR_setregid)
    SYSCALL_NAME_ITEM (setregid),
#endif 

#if defined(__NR_sigsuspend)
    SYSCALL_NAME_ITEM (kernel_signal_suspend),
#endif 

#if defined(__NR_sigpending)
    SYSCALL_NAME_ITEM (sigpending),
#endif 

#if defined(__NR_sethostname)
    SYSCALL_NAME_ITEM (sethostname),
#endif 

#if defined(__NR_setrlimit)
    SYSCALL_NAME_ITEM (setrlimit),
#endif 

#if defined(__NR_getrlimit)
    SYSCALL_NAME_ITEM (getrlimit),
#endif 

#if defined(__NR_getrusage)
    SYSCALL_NAME_ITEM (getrusage),
#endif 

#if defined(__NR_gettimeofday_time32)
    SYSCALL_NAME_ITEM (gettimeofday_time32),
#endif 

#if defined(__NR_gettimeofday)
    SYSCALL_NAME_ITEM (gettimeofday),
#endif 

#if defined(__NR_settimeofday_time32)
    SYSCALL_NAME_ITEM (settimeofday_time32),
#endif 

#if defined(__NR_settimeofday)
    SYSCALL_NAME_ITEM (settimeofday),
#endif 

#if defined(__NR_getgroups)
    SYSCALL_NAME_ITEM (getgroups),
#endif 

#if defined(__NR_setgroups)
    SYSCALL_NAME_ITEM (setgroups),
#endif 

#if defined(__NR_select)
    SYSCALL_NAME_ITEM (select),
#endif 

#if defined(__NR_symlink)
    SYSCALL_NAME_ITEM (symlink),
#endif 

#if defined(__NR_oldlstat)
    SYSCALL_NAME_ITEM (oldlstat),
#endif 

#if defined(__NR_readlink)
    SYSCALL_NAME_ITEM (readlink),
#endif 

#if defined(__NR_uselib)
    SYSCALL_NAME_ITEM (uselib),
#endif 

#if defined(__NR_swapon)
    SYSCALL_NAME_ITEM (swapon),
#endif 

#if defined(__NR_reboot)
    SYSCALL_NAME_ITEM (reboot),
#endif 

#if defined(__NR_readdir)
    SYSCALL_NAME_ITEM (readdir),
#endif 

#if defined(__NR_mmap)
    SYSCALL_NAME_ITEM (mmap),
#endif 

#if defined(__NR_munmap)
    SYSCALL_NAME_ITEM (munmap),
#endif 

#if defined(__NR_truncate)
    SYSCALL_NAME_ITEM (truncate),
#endif 

#if defined(__NR_ftruncate)
    SYSCALL_NAME_ITEM (ftruncate),
#endif 

#if defined(__NR_fchmod)
    SYSCALL_NAME_ITEM (fchmod),
#endif 

#if defined(__NR_fchown)
    SYSCALL_NAME_ITEM (fchown),
#endif 

#if defined(__NR_getpriority)
    SYSCALL_NAME_ITEM (getpriority),
#endif 

#if defined(__NR_setpriority)
    SYSCALL_NAME_ITEM (setpriority),
#endif 

#if defined(__NR_profil)
    SYSCALL_NAME_ITEM (profil),
#endif 

#if defined(__NR_statfs)
    SYSCALL_NAME_ITEM (statfs),
#endif 

#if defined(__NR_fstatfs)
    SYSCALL_NAME_ITEM (fstatfs),
#endif 

#if defined(__NR_ioperm)
    SYSCALL_NAME_ITEM (ioperm),
#endif 

#if defined(__NR_socketcall)
    SYSCALL_NAME_ITEM (socketcall),
#endif 

#if defined(__NR_syslog)
    SYSCALL_NAME_ITEM (syslog),
#endif 

#if defined(__NR_setitimer)
    SYSCALL_NAME_ITEM (setitimer),
#endif 

#if defined(__NR_getitimer)
    SYSCALL_NAME_ITEM (getitimer),
#endif 

#if defined(__NR_stat)
    SYSCALL_NAME_ITEM (stat),
#endif 

#if defined(__NR_lstat)
    SYSCALL_NAME_ITEM (lstat),
#endif 

#if defined(__NR_fstat)
    SYSCALL_NAME_ITEM (fstat),
#endif 

#if defined(__NR_olduname)
    SYSCALL_NAME_ITEM (olduname),
#endif 

#if defined(__NR_iopl)
    SYSCALL_NAME_ITEM (iopl),
#endif 

#if defined(__NR_vhangup)
    SYSCALL_NAME_ITEM (vhangup),
#endif 

#if defined(__NR_idle)
    SYSCALL_NAME_ITEM (idle),
#endif 

#if defined(__NR_vm86old)
    SYSCALL_NAME_ITEM (vm86old),
#endif 

#if defined(__NR_wait4)
    SYSCALL_NAME_ITEM (wait4),
#endif 

#if defined(__NR_swapoff)
    SYSCALL_NAME_ITEM (swapoff),
#endif 

#if defined(__NR_sysinfo)
    SYSCALL_NAME_ITEM (sysinfo),
#endif 

#if defined(__NR_ipc)
    SYSCALL_NAME_ITEM (ipc),
#endif 

#if defined(__NR_fsync)
    SYSCALL_NAME_ITEM (fsync),
#endif 

#if defined(__NR_sigreturn)
    SYSCALL_NAME_ITEM (sigreturn),
#endif 

#if defined(__NR_clone)
    SYSCALL_NAME_ITEM (clone),
#endif 

#if defined(__NR_setdomainname)
    SYSCALL_NAME_ITEM (setdomainname),
#endif 

#if defined(__NR_uname)
    SYSCALL_NAME_ITEM (uname),
#endif 

#if defined(__NR_modify_ldt)
    SYSCALL_NAME_ITEM (modify_ldt),
#endif 

#if defined(__NR_adjtimex)
    SYSCALL_NAME_ITEM (adjtimex),
#endif 

#if defined(__NR_mprotect)
    SYSCALL_NAME_ITEM (mprotect),
#endif 

#if defined(__NR_sigprocmask)
    SYSCALL_NAME_ITEM (sigprocmask),
#endif 

#if defined(__NR_quotactl)
    SYSCALL_NAME_ITEM (quotactl),
#endif 

#if defined(__NR_getpgid)
    SYSCALL_NAME_ITEM (getpgid),
#endif 

#if defined(__NR_fchdir)
    SYSCALL_NAME_ITEM (fchdir),
#endif 

#if defined(__NR_bdflush)
    SYSCALL_NAME_ITEM (bdflush),
#endif 

#if defined(__NR_sysfs)
    SYSCALL_NAME_ITEM (sysfs),
#endif 

#if defined(__NR_personality)
    SYSCALL_NAME_ITEM (personality),
#endif 

#if defined(__NR_afs_syscall)
    SYSCALL_NAME_ITEM (afs_syscall),
#endif 

#if defined(__NR_setfsuid)
    SYSCALL_NAME_ITEM (setfsuid),
#endif 

#if defined(__NR_setfsgid)
    SYSCALL_NAME_ITEM (setfsgid),
#endif 

#if defined(__NR__llseek)
    SYSCALL_NAME_ITEM (_llseek),
#endif 

#if defined(__NR_getdents)
    SYSCALL_NAME_ITEM (getdents),
#endif 

#if defined(__NR__newselect)
    SYSCALL_NAME_ITEM (_newselect),
#endif 

#if defined(__NR_flock)
    SYSCALL_NAME_ITEM (flock),
#endif 

#if defined(__NR_msync)
    SYSCALL_NAME_ITEM (msync),
#endif 

#if defined(__NR_readv)
    SYSCALL_NAME_ITEM (readv),
#endif 

#if defined(__NR_writev)
    SYSCALL_NAME_ITEM (writev),
#endif 

#if defined(__NR_getsid)
    SYSCALL_NAME_ITEM (getsid),
#endif 

#if defined(__NR_fdatasync)
    SYSCALL_NAME_ITEM (fdatasync),
#endif 

#if defined(__NR__sysctl)
    SYSCALL_NAME_ITEM (_sysctl),
#endif 

#if defined(__NR_mlock)
    SYSCALL_NAME_ITEM (mlock),
#endif 

#if defined(__NR_munlock)
    SYSCALL_NAME_ITEM (munlock),
#endif 

#if defined(__NR_mlockall)
    SYSCALL_NAME_ITEM (mlockall),
#endif 

#if defined(__NR_munlockall)
    SYSCALL_NAME_ITEM (munlockall),
#endif 

#if defined(__NR_sched_setparam)
    SYSCALL_NAME_ITEM (sched_setparam),
#endif 

#if defined(__NR_sched_getparam)
    SYSCALL_NAME_ITEM (sched_getparam),
#endif 

#if defined(__NR_sched_setscheduler)
    SYSCALL_NAME_ITEM (sched_setscheduler),
#endif 

#if defined(__NR_sched_getscheduler)
    SYSCALL_NAME_ITEM (sched_getscheduler),
#endif 

#if defined(__NR_sched_yield)
    SYSCALL_NAME_ITEM (sched_yield),
#endif 

#if defined(__NR_sched_get_priority_max)
    SYSCALL_NAME_ITEM (sched_get_priority_max),
#endif 

#if defined(__NR_sched_get_priority_min)
    SYSCALL_NAME_ITEM (sched_get_priority_min),
#endif 

#if defined(__NR_sched_rr_get_interval)
    SYSCALL_NAME_ITEM (sched_rr_get_interval),
#endif 

#if defined(__NR_nanosleep)
    SYSCALL_NAME_ITEM (nanosleep),
#endif 

#if defined(__NR_mremap)
    SYSCALL_NAME_ITEM (mremap),
#endif 

#if defined(__NR_setresuid)
    SYSCALL_NAME_ITEM (setresuid),
#endif 

#if defined(__NR_getresuid)
    SYSCALL_NAME_ITEM (getresuid),
#endif 

#if defined(__NR_vm86)
    SYSCALL_NAME_ITEM (vm86),
#endif 

#if defined(__NR_query_module)
    SYSCALL_NAME_ITEM (query_module),
#endif 

#if defined(__NR_poll)
    SYSCALL_NAME_ITEM (poll),
#endif 

#if defined(__NR_nfsservctl)
    SYSCALL_NAME_ITEM (nfsservctl),
#endif 

#if defined(__NR_setresgid)
    SYSCALL_NAME_ITEM (setresgid),
#endif 

#if defined(__NR_getresgid)
    SYSCALL_NAME_ITEM (getresgid),
#endif 

#if defined(__NR_prctl)
    SYSCALL_NAME_ITEM (prctl),
#endif 

#if defined(__NR_rt_sigreturn)
    SYSCALL_NAME_ITEM (rt_sigreturn),
#endif 

#if defined(__NR_rt_sigaction)
    SYSCALL_NAME_ITEM (rt_sigaction),
#endif 

#if defined(__NR_rt_sigprocmask)
    SYSCALL_NAME_ITEM (rt_sigprocmask),
#endif 

#if defined(__NR_rt_sigpending)
    SYSCALL_NAME_ITEM (rt_sigpending),
#endif 

#if defined(__NR_rt_sigtimedwait)
    SYSCALL_NAME_ITEM (rt_sigtimedwait),
#endif 

#if defined(__NR_rt_sigqueueinfo)
    SYSCALL_NAME_ITEM (rt_sigqueueinfo),
#endif 

#if defined(__NR_rt_sigsuspend)
    SYSCALL_NAME_ITEM (rt_sigsuspend),
#endif 

#if defined(__NR_pread64)
    SYSCALL_NAME_ITEM (pread64),
#endif 

#if defined(__NR_pwrite64)
    SYSCALL_NAME_ITEM (pwrite64),
#endif 

#if defined(__NR_chown)
    SYSCALL_NAME_ITEM (chown),
#endif 

#if defined(__NR_getcwd)
    SYSCALL_NAME_ITEM (getcwd),
#endif 

#if defined(__NR_capget)
    SYSCALL_NAME_ITEM (capget),
#endif 

#if defined(__NR_capset)
    SYSCALL_NAME_ITEM (capset),
#endif 

#if defined(__NR_sigaltstack)
    SYSCALL_NAME_ITEM (sigaltstack),
#endif 

#if defined(__NR_sendfile)
    SYSCALL_NAME_ITEM (sendfile),
#endif 

#if defined(__NR_getpmsg)
    SYSCALL_NAME_ITEM (getpmsg),
#endif 

#if defined(__NR_putpmsg)
    SYSCALL_NAME_ITEM (putpmsg),
#endif 

#if defined(__NR_vfork)
    SYSCALL_NAME_ITEM (vfork),
#endif 

#if defined(__NR_ugetrlimit)
    SYSCALL_NAME_ITEM (ugetrlimit),
#endif 

#if defined(__NR_mmap2)
    SYSCALL_NAME_ITEM (mmap2),
#endif 

#if defined(__NR_truncate64)
    SYSCALL_NAME_ITEM (truncate64),
#endif 

#if defined(__NR_ftruncate64)
    SYSCALL_NAME_ITEM (ftruncate64),
#endif 

#if defined(__NR_stat64)
    SYSCALL_NAME_ITEM (stat64),
#endif 

#if defined(__NR_lstat64)
    SYSCALL_NAME_ITEM (lstat64),
#endif 

#if defined(__NR_fstat64)
    SYSCALL_NAME_ITEM (fstat64),
#endif 

#if defined(__NR_lchown32)
    SYSCALL_NAME_ITEM (lchown32),
#endif 

#if defined(__NR_getuid32)
    SYSCALL_NAME_ITEM (getuid32),
#endif 

#if defined(__NR_getgid32)
    SYSCALL_NAME_ITEM (getgid32),
#endif 

#if defined(__NR_geteuid32)
    SYSCALL_NAME_ITEM (geteuid32),
#endif 

#if defined(__NR_getegid32)
    SYSCALL_NAME_ITEM (getegid32),
#endif 

#if defined(__NR_setreuid32)
    SYSCALL_NAME_ITEM (setreuid32),
#endif 

#if defined(__NR_setregid32)
    SYSCALL_NAME_ITEM (setregid32),
#endif 

#if defined(__NR_getgroups32)
    SYSCALL_NAME_ITEM (getgroups32),
#endif 

#if defined(__NR_setgroups32)
    SYSCALL_NAME_ITEM (setgroups32),
#endif 

#if defined(__NR_fchown32)
    SYSCALL_NAME_ITEM (fchown32),
#endif 

#if defined(__NR_setresuid32)
    SYSCALL_NAME_ITEM (setresuid32),
#endif 

#if defined(__NR_getresuid32)
    SYSCALL_NAME_ITEM (getresuid32),
#endif 

#if defined(__NR_setresgid32)
    SYSCALL_NAME_ITEM (setresgid32),
#endif 

#if defined(__NR_getresgid32)
    SYSCALL_NAME_ITEM (getresgid32),
#endif 

#if defined(__NR_chown32)
    SYSCALL_NAME_ITEM (chown32),
#endif 

#if defined(__NR_setuid32)
    SYSCALL_NAME_ITEM (setuid32),
#endif 

#if defined(__NR_setgid32)
    SYSCALL_NAME_ITEM (setgid32),
#endif 

#if defined(__NR_setfsuid32)
    SYSCALL_NAME_ITEM (setfsuid32),
#endif 

#if defined(__NR_setfsgid32)
    SYSCALL_NAME_ITEM (setfsgid32),
#endif 

#if defined(__NR_pivot_root)
    SYSCALL_NAME_ITEM (pivot_root),
#endif 

#if defined(__NR_mincore)
    SYSCALL_NAME_ITEM (mincore),
#endif 

#if defined(__NR_madvise)
    SYSCALL_NAME_ITEM (madvise),
#endif 

#if defined(__NR_getdents64)
    SYSCALL_NAME_ITEM (getdents64),
#endif 

#if defined(__NR_fcntl64)
    SYSCALL_NAME_ITEM (fcntl64),
#endif 

#if defined(__NR_gettid)
    SYSCALL_NAME_ITEM (gettid),
#endif 

#if defined(__NR_readahead)
    SYSCALL_NAME_ITEM (readahead),
#endif 

#if defined(__NR_setxattr)
    SYSCALL_NAME_ITEM (setxattr),
#endif 

#if defined(__NR_lsetxattr)
    SYSCALL_NAME_ITEM (lsetxattr),
#endif 

#if defined(__NR_fsetxattr)
    SYSCALL_NAME_ITEM (fsetxattr),
#endif 

#if defined(__NR_getxattr)
    SYSCALL_NAME_ITEM (getxattr),
#endif 

#if defined(__NR_lgetxattr)
    SYSCALL_NAME_ITEM (lgetxattr),
#endif 

#if defined(__NR_fgetxattr)
    SYSCALL_NAME_ITEM (fgetxattr),
#endif 

#if defined(__NR_listxattr)
    SYSCALL_NAME_ITEM (listxattr),
#endif 

#if defined(__NR_llistxattr)
    SYSCALL_NAME_ITEM (llistxattr),
#endif 

#if defined(__NR_flistxattr)
    SYSCALL_NAME_ITEM (flistxattr),
#endif 

#if defined(__NR_removexattr)
    SYSCALL_NAME_ITEM (removexattr),
#endif 

#if defined(__NR_lremovexattr)
    SYSCALL_NAME_ITEM (lremovexattr),
#endif 

#if defined(__NR_fremovexattr)
    SYSCALL_NAME_ITEM (fremovexattr),
#endif 

#if defined(__NR_tkill)
    SYSCALL_NAME_ITEM (tkill),
#endif 

#if defined(__NR_sendfile64)
    SYSCALL_NAME_ITEM (sendfile64),
#endif 

#if defined(__NR_futex)
    SYSCALL_NAME_ITEM (futex),
#endif 

#if defined(__NR_sched_setaffinity)
    SYSCALL_NAME_ITEM (sched_setaffinity),
#endif 

#if defined(__NR_sched_getaffinity)
    SYSCALL_NAME_ITEM (sched_getaffinity),
#endif 

#if defined(__NR_set_thread_area)
    SYSCALL_NAME_ITEM (set_thread_area),
#endif 

#if defined(__NR_get_thread_area)
    SYSCALL_NAME_ITEM (get_thread_area),
#endif 

#if defined(__NR_io_setup)
    SYSCALL_NAME_ITEM (io_setup),
#endif 

#if defined(__NR_io_destroy)
    SYSCALL_NAME_ITEM (io_destroy),
#endif 

#if defined(__NR_io_getevents)
    SYSCALL_NAME_ITEM (io_getevents),
#endif 

#if defined(__NR_io_submit)
    SYSCALL_NAME_ITEM (io_submit),
#endif 

#if defined(__NR_io_cancel)
    SYSCALL_NAME_ITEM (io_cancel),
#endif 

#if defined(__NR_fadvise64)
    SYSCALL_NAME_ITEM (fadvise64),
#endif 

#if defined(__NR_exit_group)
    SYSCALL_NAME_ITEM (exit_group),
#endif 

#if defined(__NR_lookup_dcookie)
    SYSCALL_NAME_ITEM (lookup_dcookie),
#endif 

#if defined(__NR_epoll_create)
    SYSCALL_NAME_ITEM (epoll_create),
#endif 

#if defined(__NR_epoll_ctl)
    SYSCALL_NAME_ITEM (epoll_ctl),
#endif 

#if defined(__NR_epoll_wait)
    SYSCALL_NAME_ITEM (epoll_wait),
#endif 

#if defined(__NR_remap_file_pages)
    SYSCALL_NAME_ITEM (remap_file_pages),
#endif 

#if defined(__NR_set_tid_address)
    SYSCALL_NAME_ITEM (set_tid_address),
#endif 

#if defined(__NR_timer_create)
    SYSCALL_NAME_ITEM (timer_create),
#endif 

#if defined(__NR_timer_settime32)
    SYSCALL_NAME_ITEM (timer_settime32),
#endif 

#if defined(__NR_timer_settime)
    SYSCALL_NAME_ITEM (timer_settime),
#endif 

#if defined(__NR_timer_gettime32)
    SYSCALL_NAME_ITEM (timer_gettime32),
#endif 

#if defined(__NR_timer_gettime)
    SYSCALL_NAME_ITEM (timer_gettime),
#endif 

#if defined(__NR_timer_getoverrun)
    SYSCALL_NAME_ITEM (timer_getoverrun),
#endif 

#if defined(__NR_timer_delete)
    SYSCALL_NAME_ITEM (timer_delete),
#endif 

#if defined(__NR_clock_settime32)
    SYSCALL_NAME_ITEM (clock_settime32),
#endif 

#if defined(__NR_clock_gettime32)
    SYSCALL_NAME_ITEM (clock_gettime32),
#endif 

#if defined(__NR_clock_gettime)
    SYSCALL_NAME_ITEM (clock_gettime),
#endif

#if defined(__NR_clock_getres_time32)
    SYSCALL_NAME_ITEM (clock_getres_time32),
#endif

#if defined(__NR_clock_nanosleep_time32)
    SYSCALL_NAME_ITEM (clock_nanosleep_time32),
#endif

#if defined(__NR_statfs64)
    SYSCALL_NAME_ITEM (statfs64),
#endif 

#if defined(__NR_fstatfs64)
    SYSCALL_NAME_ITEM (fstatfs64),
#endif 

#if defined(__NR_tgkill)
    SYSCALL_NAME_ITEM (tgkill),
#endif 

#if defined(__NR_utimes)
    SYSCALL_NAME_ITEM (utimes),
#endif 

#if defined(__NR_fadvise64_64)
    SYSCALL_NAME_ITEM (fadvise64_64),
#endif 

#if defined(__NR_vserver)
    SYSCALL_NAME_ITEM (vserver),
#endif 

#if defined(__NR_mbind)
    SYSCALL_NAME_ITEM (mbind),
#endif 

#if defined(__NR_get_mempolicy)
    SYSCALL_NAME_ITEM (get_mempolicy),
#endif 

#if defined(__NR_set_mempolicy)
    SYSCALL_NAME_ITEM (set_mempolicy),
#endif 

#if defined(__NR_mq_open)
    SYSCALL_NAME_ITEM (mq_open),
#endif 

#if defined(__NR_mq_unlink)
    SYSCALL_NAME_ITEM (mq_unlink),
#endif 

#if defined(__NR_mq_timedsend)
    SYSCALL_NAME_ITEM (mq_timedsend),
#endif 

#if defined(__NR_mq_timedreceive)
    SYSCALL_NAME_ITEM (mq_timedreceive),
#endif 

#if defined(__NR_mq_notify)
    SYSCALL_NAME_ITEM (mq_notify),
#endif 

#if defined(__NR_mq_getsetattr)
    SYSCALL_NAME_ITEM (mq_getsetattr),
#endif 

#if defined(__NR_kexec_load)
    SYSCALL_NAME_ITEM (kexec_load),
#endif 

#if defined(__NR_waitid)
    SYSCALL_NAME_ITEM (waitid),
#endif 

#if defined(__NR_add_key)
    SYSCALL_NAME_ITEM (add_key),
#endif 

#if defined(__NR_request_key)
    SYSCALL_NAME_ITEM (request_key),
#endif 

#if defined(__NR_keyctl)
    SYSCALL_NAME_ITEM (keyctl),
#endif 

#if defined(__NR_ioprio_set)
    SYSCALL_NAME_ITEM (ioprio_set),
#endif 

#if defined(__NR_ioprio_get)
    SYSCALL_NAME_ITEM (ioprio_get),
#endif 

#if defined(__NR_inotify_init)
    SYSCALL_NAME_ITEM (inotify_init),
#endif 

#if defined(__NR_inotify_add_watch)
    SYSCALL_NAME_ITEM (inotify_add_watch),
#endif 

#if defined(__NR_inotify_rm_watch)
    SYSCALL_NAME_ITEM (inotify_rm_watch),
#endif 

#if defined(__NR_migrate_pages)
    SYSCALL_NAME_ITEM (migrate_pages),
#endif 

#if defined(__NR_openat)
    SYSCALL_NAME_ITEM (openat),
#endif 

#if defined(__NR_mkdirat)
    SYSCALL_NAME_ITEM (mkdirat),
#endif 

#if defined(__NR_mknodat)
    SYSCALL_NAME_ITEM (mknodat),
#endif 

#if defined(__NR_fchownat)
    SYSCALL_NAME_ITEM (fchownat),
#endif 

#if defined(__NR_futimesat)
    SYSCALL_NAME_ITEM (futimesat),
#endif 

#if defined(__NR_fstatat64)
    SYSCALL_NAME_ITEM (fstatat64),
#endif 

#if defined(__NR_newfstatat)
    SYSCALL_NAME_ITEM (newfstatat),
#endif 

#if defined(__NR_unlinkat)
    SYSCALL_NAME_ITEM (unlinkat),
#endif 

#if defined(__NR_renameat)
    SYSCALL_NAME_ITEM (renameat),
#endif 

#if defined(__NR_linkat)
    SYSCALL_NAME_ITEM (linkat),
#endif 

#if defined(__NR_symlinkat)
    SYSCALL_NAME_ITEM (symlinkat),
#endif 

#if defined(__NR_readlinkat)
    SYSCALL_NAME_ITEM (readlinkat),
#endif 

#if defined(__NR_fchmodat)
    SYSCALL_NAME_ITEM (fchmodat),
#endif 

#if defined(__NR_faccessat)
    SYSCALL_NAME_ITEM (faccessat),
#endif 

#if defined(__NR_pselect6)
    SYSCALL_NAME_ITEM (pselect6),
#endif 

#if defined(__NR_ppoll)
    SYSCALL_NAME_ITEM (ppoll),
#endif 

#if defined(__NR_unshare)
    SYSCALL_NAME_ITEM (unshare),
#endif 

#if defined(__NR_set_robust_list)
    SYSCALL_NAME_ITEM (set_robust_list),
#endif 

#if defined(__NR_get_robust_list)
    SYSCALL_NAME_ITEM (get_robust_list),
#endif 

#if defined(__NR_splice)
    SYSCALL_NAME_ITEM (splice),
#endif 

#if defined(__NR_sync_file_range)
    SYSCALL_NAME_ITEM (sync_file_range),
#endif 

#if defined(__NR_tee)
    SYSCALL_NAME_ITEM (tee),
#endif 

#if defined(__NR_vmsplice)
    SYSCALL_NAME_ITEM (vmsplice),
#endif 

#if defined(__NR_move_pages)
    SYSCALL_NAME_ITEM (move_pages),
#endif 

#if defined(__NR_getcpu)
    SYSCALL_NAME_ITEM (getcpu),
#endif 

#if defined(__NR_epoll_pwait)
    SYSCALL_NAME_ITEM (epoll_pwait),
#endif 

#if defined(__NR_utimensat)
    SYSCALL_NAME_ITEM (utimensat),
#endif 

#if defined(__NR_signalfd)
    SYSCALL_NAME_ITEM (signalfd),
#endif 

#if defined(__NR_timerfd_create)
    SYSCALL_NAME_ITEM (timerfd_create),
#endif 

#if defined(__NR_eventfd)
    SYSCALL_NAME_ITEM (eventfd),
#endif 

#if defined(__NR_fallocate)
    SYSCALL_NAME_ITEM (fallocate),
#endif 

#if defined(__NR_timerfd_settime32)
    SYSCALL_NAME_ITEM (timerfd_settime32),
#endif 

#if defined(__NR_timerfd_gettime32)
    SYSCALL_NAME_ITEM (timerfd_gettime32),
#endif 

#if defined(__NR_signalfd4)
    SYSCALL_NAME_ITEM (signalfd4),
#endif 

#if defined(__NR_eventfd2)
    SYSCALL_NAME_ITEM (eventfd2),
#endif 

#if defined(__NR_epoll_create1)
    SYSCALL_NAME_ITEM (epoll_create1),
#endif 

#if defined(__NR_dup3)
    SYSCALL_NAME_ITEM (dup3),
#endif 

#if defined(__NR_pipe2)
    SYSCALL_NAME_ITEM (pipe2),
#endif 

#if defined(__NR_inotify_init1)
    SYSCALL_NAME_ITEM (inotify_init1),
#endif 

#if defined(__NR_preadv)
    SYSCALL_NAME_ITEM (preadv),
#endif 

#if defined(__NR_pwritev)
    SYSCALL_NAME_ITEM (pwritev),
#endif 

#if defined(__NR_rt_tgsigqueueinfo)
    SYSCALL_NAME_ITEM (rt_tgsigqueueinfo),
#endif 

#if defined(__NR_perf_event_open)
    SYSCALL_NAME_ITEM (perf_event_open),
#endif 

#if defined(__NR_recvmmsg)
    SYSCALL_NAME_ITEM (recvmmsg),
#endif 

#if defined(__NR_fanotify_init)
    SYSCALL_NAME_ITEM (fanotify_init),
#endif 

#if defined(__NR_fanotify_mark)
    SYSCALL_NAME_ITEM (fanotify_mark),
#endif 

#if defined(__NR_prlimit64)
    SYSCALL_NAME_ITEM (prlimit64),
#endif 

#if defined(__NR_name_to_handle_at)
    SYSCALL_NAME_ITEM (name_to_handle_at),
#endif 

#if defined(__NR_open_by_handle_at)
    SYSCALL_NAME_ITEM (open_by_handle_at),
#endif 

#if defined(__NR_clock_adjtime)
    SYSCALL_NAME_ITEM (clock_adjtime),
#endif 

#if defined(__NR_syncfs)
    SYSCALL_NAME_ITEM (syncfs),
#endif 

#if defined(__NR_sendmmsg)
    SYSCALL_NAME_ITEM (sendmmsg),
#endif 

#if defined(__NR_setns)
    SYSCALL_NAME_ITEM (setns),
#endif 

#if defined(__NR_process_vm_readv)
    SYSCALL_NAME_ITEM (process_vm_readv),
#endif 

#if defined(__NR_process_vm_writev)
    SYSCALL_NAME_ITEM (process_vm_writev),
#endif 

#if defined(__NR_kcmp)
    SYSCALL_NAME_ITEM (kcmp),
#endif 

#if defined(__NR_sched_setattr)
    SYSCALL_NAME_ITEM (sched_setattr),
#endif 

#if defined(__NR_sched_getattr)
    SYSCALL_NAME_ITEM (sched_getattr),
#endif 

#if defined(__NR_renameat2)
    SYSCALL_NAME_ITEM (renameat2),
#endif 

#if defined(__NR_seccomp)
    SYSCALL_NAME_ITEM (seccomp),
#endif 

#if defined(__NR_getrandom)
    SYSCALL_NAME_ITEM (getrandom),
#endif 

#if defined(__NR_memfd_create)
    SYSCALL_NAME_ITEM (memfd_create),
#endif 

#if defined(__NR_bpf)
    SYSCALL_NAME_ITEM (bpf),
#endif 

#if defined(__NR_execveat)
    SYSCALL_NAME_ITEM (execveat),
#endif 

#if defined(__NR_socket)
    SYSCALL_NAME_ITEM (socket),
#endif 

#if defined(__NR_socketpair)
    SYSCALL_NAME_ITEM (socketpair),
#endif 

#if defined(__NR_bind)
    SYSCALL_NAME_ITEM (bind),
#endif 

#if defined(__NR_connect)
    SYSCALL_NAME_ITEM (connect),
#endif 

#if defined(__NR_listen)
    SYSCALL_NAME_ITEM (listen),
#endif 

#if defined(__NR_accept)
    SYSCALL_NAME_ITEM (accept),
#endif 

#if defined(__NR_accept4)
    SYSCALL_NAME_ITEM (accept4),
#endif 

#if defined(__NR_getsockopt)
    SYSCALL_NAME_ITEM (getsockopt),
#endif 

#if defined(__NR_setsockopt)
    SYSCALL_NAME_ITEM (setsockopt),
#endif 

#if defined(__NR_getsockname)
    SYSCALL_NAME_ITEM (getsockname),
#endif 

#if defined(__NR_getpeername)
    SYSCALL_NAME_ITEM (getpeername),
#endif 

#if defined(__NR_sendto)
    SYSCALL_NAME_ITEM (sendto),
#endif 

#if defined(__NR_sendmsg)
    SYSCALL_NAME_ITEM (sendmsg),
#endif 

#if defined(__NR_recvfrom)
    SYSCALL_NAME_ITEM (recvfrom),
#endif 

#if defined(__NR_recvmsg)
    SYSCALL_NAME_ITEM (recvmsg),
#endif 

#if defined(__NR_shutdown)
    SYSCALL_NAME_ITEM (shutdown),
#endif 

#if defined(__NR_userfaultfd)
    SYSCALL_NAME_ITEM (userfaultfd),
#endif 

#if defined(__NR_membarrier)
    SYSCALL_NAME_ITEM (membarrier),
#endif 

#if defined(__NR_mlock2)
    SYSCALL_NAME_ITEM (mlock2),
#endif 

#if defined(__NR_copy_file_range)
    SYSCALL_NAME_ITEM (copy_file_range),
#endif 

#if defined(__NR_preadv2)
    SYSCALL_NAME_ITEM (preadv2),
#endif 

#if defined(__NR_pwritev2)
    SYSCALL_NAME_ITEM (pwritev2),
#endif 

#if defined(__NR_pkey_mprotect)
    SYSCALL_NAME_ITEM (pkey_mprotect),
#endif 

#if defined(__NR_pkey_alloc)
    SYSCALL_NAME_ITEM (pkey_alloc),
#endif 

#if defined(__NR_pkey_free)
    SYSCALL_NAME_ITEM (pkey_free),
#endif 

#if defined(__NR_statx)
    SYSCALL_NAME_ITEM (statx),
#endif 

#if defined(__NR_arch_prctl)
    SYSCALL_NAME_ITEM (arch_prctl),
#endif 

#if defined(__NR_io_pgetevents)
    SYSCALL_NAME_ITEM (io_pgetevents),
#endif 

#if defined(__NR_rseq)
    SYSCALL_NAME_ITEM (rseq),
#endif 

#if defined(__NR_semget)
    SYSCALL_NAME_ITEM (semget),
#endif 

#if defined(__NR_semctl)
    SYSCALL_NAME_ITEM (semctl),
#endif 

#if defined(__NR_shmget)
    SYSCALL_NAME_ITEM (shmget),
#endif 

#if defined(__NR_shmctl)
    SYSCALL_NAME_ITEM (shmctl),
#endif 

#if defined(__NR_shmat)
    SYSCALL_NAME_ITEM (shmat),
#endif 

#if defined(__NR_shmdt)
    SYSCALL_NAME_ITEM (shmdt),
#endif 

#if defined(__NR_msgget)
    SYSCALL_NAME_ITEM (msgget),
#endif 

#if defined(__NR_msgsnd)
    SYSCALL_NAME_ITEM (msgsnd),
#endif 

#if defined(__NR_msgrcv)
    SYSCALL_NAME_ITEM (msgrcv),
#endif 

#if defined(__NR_msgctl)
    SYSCALL_NAME_ITEM (msgctl),
#endif 

#if defined(__NR_clock_gettime64)
    SYSCALL_NAME_ITEM (clock_gettime64),
#endif 

#if defined(__NR_clock_settime64)
    SYSCALL_NAME_ITEM (clock_settime64),
#endif 

#if defined(__NR_clock_adjtime64)
    SYSCALL_NAME_ITEM (clock_adjtime64),
#endif 

#if defined(__NR_clock_getres)
    SYSCALL_NAME_ITEM (clock_getres),
#endif 

#if defined(__NR_clock_getres_time64)
    SYSCALL_NAME_ITEM (clock_getres_time64),
#endif 

#if defined(__NR_clock_nanosleep)
    SYSCALL_NAME_ITEM (clock_nanosleep),
#endif 

#if defined(__NR_clock_nanosleep_time64)
    SYSCALL_NAME_ITEM (clock_nanosleep_time64),
#endif 

#if defined(__NR_timer_gettime64)
    SYSCALL_NAME_ITEM (timer_gettime64),
#endif 

#if defined(__NR_timer_settime64)
    SYSCALL_NAME_ITEM (timer_settime64),
#endif 

#if defined(__NR_timerfd_gettime64)
    SYSCALL_NAME_ITEM (timerfd_gettime64),
#endif 

#if defined(__NR_timerfd_settime64)
    SYSCALL_NAME_ITEM (timerfd_settime64),
#endif 

#if defined(__NR_utimensat_time64)
    SYSCALL_NAME_ITEM (utimensat_time64),
#endif 

#if defined(__NR_pselect6_time64)
    SYSCALL_NAME_ITEM (pselect6_time64),
#endif 

#if defined(__NR_ppoll_time64)
    SYSCALL_NAME_ITEM (ppoll_time64),
#endif 

#if defined(__NR_io_pgetevents_time64)
    SYSCALL_NAME_ITEM (io_pgetevents_time64),
#endif 

#if defined(__NR_recvmmsg_time64)
    SYSCALL_NAME_ITEM (recvmmsg_time64),
#endif 

#if defined(__NR_mq_timedsend_time64)
    SYSCALL_NAME_ITEM (mq_timedsend_time64),
#endif 

#if defined(__NR_mq_timedreceive_time64)
    SYSCALL_NAME_ITEM (mq_timedreceive_time64),
#endif 

#if defined(__NR_semtimedop_time64)
    SYSCALL_NAME_ITEM (semtimedop_time64),
#endif 

#if defined(__NR_rt_sigtimedwait_time64)
    SYSCALL_NAME_ITEM (rt_sigtimedwait_time64),
#endif 

#if defined(__NR_futex_time64)
    SYSCALL_NAME_ITEM (futex_time64),
#endif 

#if defined(__NR_sched_rr_get_interval_time64)
    SYSCALL_NAME_ITEM (sched_rr_get_interval_time64),
#endif 

#if defined(__NR_pidfd_send_signal)
    SYSCALL_NAME_ITEM (pidfd_send_signal),
#endif 

#if defined(__NR_io_uring_setup)
    SYSCALL_NAME_ITEM (io_uring_setup),
#endif 

#if defined(__NR_io_uring_enter)
    SYSCALL_NAME_ITEM (io_uring_enter),
#endif 

#if defined(__NR_io_uring_register)
    SYSCALL_NAME_ITEM (io_uring_register),
#endif 

#if defined(__NR_open_tree)
    SYSCALL_NAME_ITEM (open_tree),
#endif 

#if defined(__NR_move_mount)
    SYSCALL_NAME_ITEM (move_mount),
#endif 

#if defined(__NR_fsopen)
    SYSCALL_NAME_ITEM (fsopen),
#endif 

#if defined(__NR_fsconfig)
    SYSCALL_NAME_ITEM (fsconfig),
#endif 

#if defined(__NR_fsmount)
    SYSCALL_NAME_ITEM (fsmount),
#endif 

#if defined(__NR_fspick)
    SYSCALL_NAME_ITEM (fspick),
#endif 

#if defined(__NR_pidfd_open)
    SYSCALL_NAME_ITEM (pidfd_open),
#endif 

#if defined(__NR_clone3)
    SYSCALL_NAME_ITEM (clone3),
#endif 

#if defined(__NR_close_range)
    SYSCALL_NAME_ITEM (close_range),
#endif 

#if defined(__NR_openat2)
    SYSCALL_NAME_ITEM (openat2),
#endif 

#if defined(__NR_pidfd_getfd)
    SYSCALL_NAME_ITEM (pidfd_getfd),
#endif 

#if defined(__NR_faccessat2)
    SYSCALL_NAME_ITEM (faccessat2),
#endif 

#if defined(__NR_process_madvise)
    SYSCALL_NAME_ITEM (process_madvise),
#endif 

#if defined(__NR_epoll_pwait2)
    SYSCALL_NAME_ITEM (epoll_pwait2),
#endif 

#if defined(__NR_mount_setattr)
    SYSCALL_NAME_ITEM (mount_setattr),
#endif 

#if defined(__NR_landlock_create_ruleset)
    SYSCALL_NAME_ITEM (landlock_create_ruleset),
#endif 

#if defined(__NR_landlock_add_rule)
    SYSCALL_NAME_ITEM (landlock_add_rule),
#endif 

    SYSCALL_NAME_ITEM (landlock_restrict_self)
};

const char *syscall_getname (long syscall_num)
{
    return syscall_name_table[syscall_num];
}

void *syscall_table[] = {
#if defined(__NR_restart_syscall)
    SYSCALL_ENTRY_ITEM (restart_syscall),
#endif 

#if defined(__NR_exit)
    SYSCALL_ENTRY_ITEM (exit),
#endif 

#if defined(__NR_fork)
    SYSCALL_ENTRY_ITEM (fork),
#endif 

#if defined(__NR_read)
    SYSCALL_ENTRY_ITEM (read),
#endif 

#if defined(__NR_write)
    SYSCALL_ENTRY_ITEM (write),
#endif 

#if defined(__NR_open)
    SYSCALL_ENTRY_ITEM (open),
#endif 

#if defined(__NR_close)
    SYSCALL_ENTRY_ITEM (close),
#endif 

#if defined(__NR_waitpid)
    SYSCALL_ENTRY_ITEM (waitpid),
#endif 

#if defined(__NR_creat)
    SYSCALL_ENTRY_ITEM (creat),
#endif 

#if defined(__NR_link)
    SYSCALL_ENTRY_ITEM (link),
#endif 

#if defined(__NR_unlink)
    SYSCALL_ENTRY_ITEM (unlink),
#endif 

#if defined(__NR_execve)
    SYSCALL_ENTRY_ITEM (execve),
#endif 

#if defined(__NR_chdir)
    SYSCALL_ENTRY_ITEM (chdir),
#endif 

#if defined(__NR_time)
    SYSCALL_ENTRY_ITEM (time),
#endif 

#if defined(__NR_mknod)
    SYSCALL_ENTRY_ITEM (mknod),
#endif 

#if defined(__NR_chmod)
    SYSCALL_ENTRY_ITEM (chmod),
#endif 

#if defined(__NR_lchown)
    SYSCALL_ENTRY_ITEM (lchown),
#endif 

#if defined(__NR_break)
    SYSCALL_ENTRY_ITEM (break),
#endif 

#if defined(__NR_oldstat)
    SYSCALL_ENTRY_ITEM (oldstat),
#endif 

#if defined(__NR_lseek)
    SYSCALL_ENTRY_ITEM (lseek),
#endif 

#if defined(__NR_getpid)
    SYSCALL_ENTRY_ITEM (getpid),
#endif 

#if defined(__NR_mount)
    SYSCALL_ENTRY_ITEM (mount),
#endif 

#if defined(__NR_umount)
    SYSCALL_ENTRY_ITEM (umount),
#endif 

#if defined(__NR_setuid)
    SYSCALL_ENTRY_ITEM (setuid),
#endif 

#if defined(__NR_getuid)
    SYSCALL_ENTRY_ITEM (getuid),
#endif 

#if defined(__NR_stime)
    SYSCALL_ENTRY_ITEM (stime),
#endif 

#if defined(__NR_alarm)
    SYSCALL_ENTRY_ITEM (alarm),
#endif 

#if defined(__NR_oldfstat)
    SYSCALL_ENTRY_ITEM (oldfstat),
#endif 

#if defined(__NR_pause)
    SYSCALL_ENTRY_ITEM (pause),
#endif 

#if defined(__NR_utime)
    SYSCALL_ENTRY_ITEM (utime),
#endif 

#if defined(__NR_stty)
    SYSCALL_ENTRY_ITEM (stty),
#endif 

#if defined(__NR_gtty)
    SYSCALL_ENTRY_ITEM (gtty),
#endif 

#if defined(__NR_access)
    SYSCALL_ENTRY_ITEM (access),
#endif 

#if defined(__NR_nice)
    SYSCALL_ENTRY_ITEM (nice),
#endif 

#if defined(__NR_ftime)
    SYSCALL_ENTRY_ITEM (ftime),
#endif 

#if defined(__NR_sync)
    SYSCALL_ENTRY_ITEM (sync),
#endif 

#if defined(__NR_kill)
    SYSCALL_ENTRY_ITEM (kill),
#endif 

#if defined(__NR_rename)
    SYSCALL_ENTRY_ITEM (rename),
#endif 

#if defined(__NR_mkdir)
    SYSCALL_ENTRY_ITEM (mkdir),
#endif 

#if defined(__NR_rmdir)
    SYSCALL_ENTRY_ITEM (rmdir),
#endif 

#if defined(__NR_dup)
    SYSCALL_ENTRY_ITEM (dup),
#endif 

#if defined(__NR_pipe)
    SYSCALL_ENTRY_ITEM (pipe),
#endif 

#if defined(__NR_times)
    SYSCALL_ENTRY_ITEM (times),
#endif 

#if defined(__NR_prof)
    SYSCALL_ENTRY_ITEM (prof),
#endif 

#if defined(__NR_brk)
    SYSCALL_ENTRY_ITEM (brk),
#endif 

#if defined(__NR_setgid)
    SYSCALL_ENTRY_ITEM (setgid),
#endif 

#if defined(__NR_getgid)
    SYSCALL_ENTRY_ITEM (getgid),
#endif 

#if defined(__NR_signal)
    SYSCALL_ENTRY_ITEM (signal),
#endif 

#if defined(__NR_geteuid)
    SYSCALL_ENTRY_ITEM (geteuid),
#endif 

#if defined(__NR_getegid)
    SYSCALL_ENTRY_ITEM (getegid),
#endif 

#if defined(__NR_acct)
    SYSCALL_ENTRY_ITEM (acct),
#endif 

#if defined(__NR_umount2)
    SYSCALL_ENTRY_ITEM (umount2),
#endif 

#if defined(__NR_lock)
    SYSCALL_ENTRY_ITEM (lock),
#endif 

#if defined(__NR_ioctl)
    SYSCALL_ENTRY_ITEM (ioctl),
#endif 

#if defined(__NR_fcntl)
    SYSCALL_ENTRY_ITEM (fcntl),
#endif 

#if defined(__NR_mpx)
    SYSCALL_ENTRY_ITEM (mpx),
#endif 

#if defined(__NR_setpgid)
    SYSCALL_ENTRY_ITEM (setpgid),
#endif 

#if defined(__NR_ulimit)
    SYSCALL_ENTRY_ITEM (ulimit),
#endif 

#if defined(__NR_oldolduname)
    SYSCALL_ENTRY_ITEM (oldolduname),
#endif 

#if defined(__NR_umask)
    SYSCALL_ENTRY_ITEM (umask),
#endif 

#if defined(__NR_chroot)
    SYSCALL_ENTRY_ITEM (chroot),
#endif 

#if defined(__NR_ustat)
    SYSCALL_ENTRY_ITEM (ustat),
#endif 

#if defined(__NR_dup2)
    SYSCALL_ENTRY_ITEM (dup2),
#endif 

#if defined(__NR_getppid)
    SYSCALL_ENTRY_ITEM (getppid),
#endif 

#if defined(__NR_getpgrp)
    SYSCALL_ENTRY_ITEM (getpgrp),
#endif 

#if defined(__NR_setsid)
    SYSCALL_ENTRY_ITEM (setsid),
#endif 

#if defined(__NR_sigaction)
    SYSCALL_ENTRY_ITEM (sigaction),
#endif 

#if defined(__NR_sgetmask)
    SYSCALL_ENTRY_ITEM (sgetmask),
#endif 

#if defined(__NR_ssetmask)
    SYSCALL_ENTRY_ITEM (ssetmask),
#endif 

#if defined(__NR_setreuid)
    SYSCALL_ENTRY_ITEM (setreuid),
#endif 

#if defined(__NR_setregid)
    SYSCALL_ENTRY_ITEM (setregid),
#endif 

#if defined(__NR_sigsuspend)
    SYSCALL_ENTRY_ITEM (kernel_signal_suspend),
#endif 

#if defined(__NR_sigpending)
    SYSCALL_ENTRY_ITEM (sigpending),
#endif 

#if defined(__NR_sethostname)
    SYSCALL_ENTRY_ITEM (sethostname),
#endif 

#if defined(__NR_setrlimit)
    SYSCALL_ENTRY_ITEM (setrlimit),
#endif 

#if defined(__NR_getrlimit)
    SYSCALL_ENTRY_ITEM (getrlimit),
#endif 

#if defined(__NR_getrusage)
    SYSCALL_ENTRY_ITEM (getrusage),
#endif 

#if defined(__NR_gettimeofday_time32)
    SYSCALL_ENTRY_ITEM (gettimeofday_time32),
#endif 

#if defined(__NR_gettimeofday)
    SYSCALL_ENTRY_ITEM (gettimeofday),
#endif 

#if defined(__NR_settimeofday_time32)
    SYSCALL_ENTRY_ITEM (settimeofday_time32),
#endif 

#if defined(__NR_settimeofday)
    SYSCALL_ENTRY_ITEM (settimeofday),
#endif 

#if defined(__NR_getgroups)
    SYSCALL_ENTRY_ITEM (getgroups),
#endif 

#if defined(__NR_setgroups)
    SYSCALL_ENTRY_ITEM (setgroups),
#endif 

#if defined(__NR_select)
    SYSCALL_ENTRY_ITEM (select),
#endif 

#if defined(__NR_symlink)
    SYSCALL_ENTRY_ITEM (symlink),
#endif 

#if defined(__NR_oldlstat)
    SYSCALL_ENTRY_ITEM (oldlstat),
#endif 

#if defined(__NR_readlink)
    SYSCALL_ENTRY_ITEM (readlink),
#endif 

#if defined(__NR_uselib)
    SYSCALL_ENTRY_ITEM (uselib),
#endif 

#if defined(__NR_swapon)
    SYSCALL_ENTRY_ITEM (swapon),
#endif 

#if defined(__NR_reboot)
    SYSCALL_ENTRY_ITEM (reboot),
#endif 

#if defined(__NR_readdir)
    SYSCALL_ENTRY_ITEM (readdir),
#endif 

#if defined(__NR_mmap)
    SYSCALL_ENTRY_ITEM (mmap),
#endif 

#if defined(__NR_munmap)
    SYSCALL_ENTRY_ITEM (munmap),
#endif 

#if defined(__NR_truncate)
    SYSCALL_ENTRY_ITEM (truncate),
#endif 

#if defined(__NR_ftruncate)
    SYSCALL_ENTRY_ITEM (ftruncate),
#endif 

#if defined(__NR_fchmod)
    SYSCALL_ENTRY_ITEM (fchmod),
#endif 

#if defined(__NR_fchown)
    SYSCALL_ENTRY_ITEM (fchown),
#endif 

#if defined(__NR_getpriority)
    SYSCALL_ENTRY_ITEM (getpriority),
#endif 

#if defined(__NR_setpriority)
    SYSCALL_ENTRY_ITEM (setpriority),
#endif 

#if defined(__NR_profil)
    SYSCALL_ENTRY_ITEM (profil),
#endif 

#if defined(__NR_statfs)
    SYSCALL_ENTRY_ITEM (statfs),
#endif 

#if defined(__NR_fstatfs)
    SYSCALL_ENTRY_ITEM (fstatfs),
#endif 

#if defined(__NR_ioperm)
    SYSCALL_ENTRY_ITEM (ioperm),
#endif 

#if defined(__NR_socketcall)
    SYSCALL_ENTRY_ITEM (socketcall),
#endif 

#if defined(__NR_syslog)
    SYSCALL_ENTRY_ITEM (syslog),
#endif 

#if defined(__NR_setitimer)
    SYSCALL_ENTRY_ITEM (setitimer),
#endif 

#if defined(__NR_getitimer)
    SYSCALL_ENTRY_ITEM (getitimer),
#endif 

#if defined(__NR_stat)
    SYSCALL_ENTRY_ITEM (stat),
#endif 

#if defined(__NR_lstat)
    SYSCALL_ENTRY_ITEM (lstat),
#endif 

#if defined(__NR_fstat)
    SYSCALL_ENTRY_ITEM (fstat),
#endif 

#if defined(__NR_olduname)
    SYSCALL_ENTRY_ITEM (olduname),
#endif 

#if defined(__NR_iopl)
    SYSCALL_ENTRY_ITEM (iopl),
#endif 

#if defined(__NR_vhangup)
    SYSCALL_ENTRY_ITEM (vhangup),
#endif 

#if defined(__NR_idle)
    SYSCALL_ENTRY_ITEM (idle),
#endif 

#if defined(__NR_vm86old)
    SYSCALL_ENTRY_ITEM (vm86old),
#endif 

#if defined(__NR_wait4)
    SYSCALL_ENTRY_ITEM (wait4),
#endif 

#if defined(__NR_swapoff)
    SYSCALL_ENTRY_ITEM (swapoff),
#endif 

#if defined(__NR_sysinfo)
    SYSCALL_ENTRY_ITEM (sysinfo),
#endif 

#if defined(__NR_ipc)
    SYSCALL_ENTRY_ITEM (ipc),
#endif 

#if defined(__NR_fsync)
    SYSCALL_ENTRY_ITEM (fsync),
#endif 

#if defined(__NR_sigreturn)
    SYSCALL_ENTRY_ITEM (sigreturn),
#endif 

#if defined(__NR_clone)
    SYSCALL_ENTRY_ITEM (clone),
#endif 

#if defined(__NR_setdomainname)
    SYSCALL_ENTRY_ITEM (setdomainname),
#endif 

#if defined(__NR_uname)
    SYSCALL_ENTRY_ITEM (uname),
#endif 

#if defined(__NR_modify_ldt)
    SYSCALL_ENTRY_ITEM (modify_ldt),
#endif 

#if defined(__NR_adjtimex)
    SYSCALL_ENTRY_ITEM (adjtimex),
#endif 

#if defined(__NR_mprotect)
    SYSCALL_ENTRY_ITEM (mprotect),
#endif 

#if defined(__NR_sigprocmask)
    SYSCALL_ENTRY_ITEM (sigprocmask),
#endif 

#if defined(__NR_quotactl)
    SYSCALL_ENTRY_ITEM (quotactl),
#endif 

#if defined(__NR_getpgid)
    SYSCALL_ENTRY_ITEM (getpgid),
#endif 

#if defined(__NR_fchdir)
    SYSCALL_ENTRY_ITEM (fchdir),
#endif 

#if defined(__NR_bdflush)
    SYSCALL_ENTRY_ITEM (bdflush),
#endif 

#if defined(__NR_sysfs)
    SYSCALL_ENTRY_ITEM (sysfs),
#endif 

#if defined(__NR_personality)
    SYSCALL_ENTRY_ITEM (personality),
#endif 

#if defined(__NR_afs_syscall)
    SYSCALL_ENTRY_ITEM (afs_syscall),
#endif 

#if defined(__NR_setfsuid)
    SYSCALL_ENTRY_ITEM (setfsuid),
#endif 

#if defined(__NR_setfsgid)
    SYSCALL_ENTRY_ITEM (setfsgid),
#endif 

#if defined(__NR__llseek)
    SYSCALL_ENTRY_ITEM (_llseek),
#endif 

#if defined(__NR_getdents)
    SYSCALL_ENTRY_ITEM (getdents),
#endif 

#if defined(__NR__newselect)
    SYSCALL_ENTRY_ITEM (_newselect),
#endif 

#if defined(__NR_flock)
    SYSCALL_ENTRY_ITEM (flock),
#endif 

#if defined(__NR_msync)
    SYSCALL_ENTRY_ITEM (msync),
#endif 

#if defined(__NR_readv)
    SYSCALL_ENTRY_ITEM (readv),
#endif 

#if defined(__NR_writev)
    SYSCALL_ENTRY_ITEM (writev),
#endif 

#if defined(__NR_getsid)
    SYSCALL_ENTRY_ITEM (getsid),
#endif 

#if defined(__NR_fdatasync)
    SYSCALL_ENTRY_ITEM (fdatasync),
#endif 

#if defined(__NR__sysctl)
    SYSCALL_ENTRY_ITEM (_sysctl),
#endif 

#if defined(__NR_mlock)
    SYSCALL_ENTRY_ITEM (mlock),
#endif 

#if defined(__NR_munlock)
    SYSCALL_ENTRY_ITEM (munlock),
#endif 

#if defined(__NR_mlockall)
    SYSCALL_ENTRY_ITEM (mlockall),
#endif 

#if defined(__NR_munlockall)
    SYSCALL_ENTRY_ITEM (munlockall),
#endif 

#if defined(__NR_sched_setparam)
    SYSCALL_ENTRY_ITEM (sched_setparam),
#endif 

#if defined(__NR_sched_getparam)
    SYSCALL_ENTRY_ITEM (sched_getparam),
#endif 

#if defined(__NR_sched_setscheduler)
    SYSCALL_ENTRY_ITEM (sched_setscheduler),
#endif 

#if defined(__NR_sched_getscheduler)
    SYSCALL_ENTRY_ITEM (sched_getscheduler),
#endif 

#if defined(__NR_sched_yield)
    SYSCALL_ENTRY_ITEM (sched_yield),
#endif 

#if defined(__NR_sched_get_priority_max)
    SYSCALL_ENTRY_ITEM (sched_get_priority_max),
#endif 

#if defined(__NR_sched_get_priority_min)
    SYSCALL_ENTRY_ITEM (sched_get_priority_min),
#endif 

#if defined(__NR_sched_rr_get_interval)
    SYSCALL_ENTRY_ITEM (sched_rr_get_interval),
#endif 

#if defined(__NR_nanosleep)
    SYSCALL_ENTRY_ITEM (nanosleep),
#endif 

#if defined(__NR_mremap)
    SYSCALL_ENTRY_ITEM (mremap),
#endif 

#if defined(__NR_setresuid)
    SYSCALL_ENTRY_ITEM (setresuid),
#endif 

#if defined(__NR_getresuid)
    SYSCALL_ENTRY_ITEM (getresuid),
#endif 

#if defined(__NR_vm86)
    SYSCALL_ENTRY_ITEM (vm86),
#endif 

#if defined(__NR_query_module)
    SYSCALL_ENTRY_ITEM (query_module),
#endif 

#if defined(__NR_poll)
    SYSCALL_ENTRY_ITEM (poll),
#endif 

#if defined(__NR_nfsservctl)
    SYSCALL_ENTRY_ITEM (nfsservctl),
#endif 

#if defined(__NR_setresgid)
    SYSCALL_ENTRY_ITEM (setresgid),
#endif 

#if defined(__NR_getresgid)
    SYSCALL_ENTRY_ITEM (getresgid),
#endif 

#if defined(__NR_prctl)
    SYSCALL_ENTRY_ITEM (prctl),
#endif 

#if defined(__NR_rt_sigreturn)
    SYSCALL_ENTRY_ITEM (rt_sigreturn),
#endif 

#if defined(__NR_rt_sigaction)
    SYSCALL_ENTRY_ITEM (rt_sigaction),
#endif 

#if defined(__NR_rt_sigprocmask)
    SYSCALL_ENTRY_ITEM (rt_sigprocmask),
#endif 

#if defined(__NR_rt_sigpending)
    SYSCALL_ENTRY_ITEM (rt_sigpending),
#endif 

#if defined(__NR_rt_sigtimedwait)
    SYSCALL_ENTRY_ITEM (rt_sigtimedwait),
#endif 

#if defined(__NR_rt_sigqueueinfo)
    SYSCALL_ENTRY_ITEM (rt_sigqueueinfo),
#endif 

#if defined(__NR_rt_sigsuspend)
    SYSCALL_ENTRY_ITEM (rt_sigsuspend),
#endif 

#if defined(__NR_pread64)
    SYSCALL_ENTRY_ITEM (pread64),
#endif 

#if defined(__NR_pwrite64)
    SYSCALL_ENTRY_ITEM (pwrite64),
#endif 

#if defined(__NR_chown)
    SYSCALL_ENTRY_ITEM (chown),
#endif 

#if defined(__NR_getcwd)
    SYSCALL_ENTRY_ITEM (getcwd),
#endif 

#if defined(__NR_capget)
    SYSCALL_ENTRY_ITEM (capget),
#endif 

#if defined(__NR_capset)
    SYSCALL_ENTRY_ITEM (capset),
#endif 

#if defined(__NR_sigaltstack)
    SYSCALL_ENTRY_ITEM (sigaltstack),
#endif 

#if defined(__NR_sendfile)
    SYSCALL_ENTRY_ITEM (sendfile),
#endif 

#if defined(__NR_getpmsg)
    SYSCALL_ENTRY_ITEM (getpmsg),
#endif 

#if defined(__NR_putpmsg)
    SYSCALL_ENTRY_ITEM (putpmsg),
#endif 

#if defined(__NR_vfork)
    SYSCALL_ENTRY_ITEM (vfork),
#endif 

#if defined(__NR_ugetrlimit)
    SYSCALL_ENTRY_ITEM (ugetrlimit),
#endif 

#if defined(__NR_mmap2)
    SYSCALL_ENTRY_ITEM (mmap2),
#endif 

#if defined(__NR_truncate64)
    SYSCALL_ENTRY_ITEM (truncate64),
#endif 

#if defined(__NR_ftruncate64)
    SYSCALL_ENTRY_ITEM (ftruncate64),
#endif 

#if defined(__NR_stat64)
    SYSCALL_ENTRY_ITEM (stat64),
#endif 

#if defined(__NR_lstat64)
    SYSCALL_ENTRY_ITEM (lstat64),
#endif 

#if defined(__NR_fstat64)
    SYSCALL_ENTRY_ITEM (fstat64),
#endif 

#if defined(__NR_lchown32)
    SYSCALL_ENTRY_ITEM (lchown32),
#endif 

#if defined(__NR_getuid32)
    SYSCALL_ENTRY_ITEM (getuid32),
#endif 

#if defined(__NR_getgid32)
    SYSCALL_ENTRY_ITEM (getgid32),
#endif 

#if defined(__NR_geteuid32)
    SYSCALL_ENTRY_ITEM (geteuid32),
#endif 

#if defined(__NR_getegid32)
    SYSCALL_ENTRY_ITEM (getegid32),
#endif 

#if defined(__NR_setreuid32)
    SYSCALL_ENTRY_ITEM (setreuid32),
#endif 

#if defined(__NR_setregid32)
    SYSCALL_ENTRY_ITEM (setregid32),
#endif 

#if defined(__NR_getgroups32)
    SYSCALL_ENTRY_ITEM (getgroups32),
#endif 

#if defined(__NR_setgroups32)
    SYSCALL_ENTRY_ITEM (setgroups32),
#endif 

#if defined(__NR_fchown32)
    SYSCALL_ENTRY_ITEM (fchown32),
#endif 

#if defined(__NR_setresuid32)
    SYSCALL_ENTRY_ITEM (setresuid32),
#endif 

#if defined(__NR_getresuid32)
    SYSCALL_ENTRY_ITEM (getresuid32),
#endif 

#if defined(__NR_setresgid32)
    SYSCALL_ENTRY_ITEM (setresgid32),
#endif 

#if defined(__NR_getresgid32)
    SYSCALL_ENTRY_ITEM (getresgid32),
#endif 

#if defined(__NR_chown32)
    SYSCALL_ENTRY_ITEM (chown32),
#endif 

#if defined(__NR_setuid32)
    SYSCALL_ENTRY_ITEM (setuid32),
#endif 

#if defined(__NR_setgid32)
    SYSCALL_ENTRY_ITEM (setgid32),
#endif 

#if defined(__NR_setfsuid32)
    SYSCALL_ENTRY_ITEM (setfsuid32),
#endif 

#if defined(__NR_setfsgid32)
    SYSCALL_ENTRY_ITEM (setfsgid32),
#endif 

#if defined(__NR_pivot_root)
    SYSCALL_ENTRY_ITEM (pivot_root),
#endif 

#if defined(__NR_mincore)
    SYSCALL_ENTRY_ITEM (mincore),
#endif 

#if defined(__NR_madvise)
    SYSCALL_ENTRY_ITEM (madvise),
#endif 

#if defined(__NR_getdents64)
    SYSCALL_ENTRY_ITEM (getdents64),
#endif 

#if defined(__NR_fcntl64)
    SYSCALL_ENTRY_ITEM (fcntl64),
#endif 

#if defined(__NR_gettid)
    SYSCALL_ENTRY_ITEM (gettid),
#endif 

#if defined(__NR_readahead)
    SYSCALL_ENTRY_ITEM (readahead),
#endif 

#if defined(__NR_setxattr)
    SYSCALL_ENTRY_ITEM (setxattr),
#endif 

#if defined(__NR_lsetxattr)
    SYSCALL_ENTRY_ITEM (lsetxattr),
#endif 

#if defined(__NR_fsetxattr)
    SYSCALL_ENTRY_ITEM (fsetxattr),
#endif 

#if defined(__NR_getxattr)
    SYSCALL_ENTRY_ITEM (getxattr),
#endif 

#if defined(__NR_lgetxattr)
    SYSCALL_ENTRY_ITEM (lgetxattr),
#endif 

#if defined(__NR_fgetxattr)
    SYSCALL_ENTRY_ITEM (fgetxattr),
#endif 

#if defined(__NR_listxattr)
    SYSCALL_ENTRY_ITEM (listxattr),
#endif 

#if defined(__NR_llistxattr)
    SYSCALL_ENTRY_ITEM (llistxattr),
#endif 

#if defined(__NR_flistxattr)
    SYSCALL_ENTRY_ITEM (flistxattr),
#endif 

#if defined(__NR_removexattr)
    SYSCALL_ENTRY_ITEM (removexattr),
#endif 

#if defined(__NR_lremovexattr)
    SYSCALL_ENTRY_ITEM (lremovexattr),
#endif 

#if defined(__NR_fremovexattr)
    SYSCALL_ENTRY_ITEM (fremovexattr),
#endif 

#if defined(__NR_tkill)
    SYSCALL_ENTRY_ITEM (tkill),
#endif 

#if defined(__NR_sendfile64)
    SYSCALL_ENTRY_ITEM (sendfile64),
#endif 

#if defined(__NR_futex)
    SYSCALL_ENTRY_ITEM (futex),
#endif 

#if defined(__NR_sched_setaffinity)
    SYSCALL_ENTRY_ITEM (sched_setaffinity),
#endif 

#if defined(__NR_sched_getaffinity)
    SYSCALL_ENTRY_ITEM (sched_getaffinity),
#endif 

#if defined(__NR_set_thread_area)
    SYSCALL_ENTRY_ITEM (set_thread_area),
#endif 

#if defined(__NR_get_thread_area)
    SYSCALL_ENTRY_ITEM (get_thread_area),
#endif 

#if defined(__NR_io_setup)
    SYSCALL_ENTRY_ITEM (io_setup),
#endif 

#if defined(__NR_io_destroy)
    SYSCALL_ENTRY_ITEM (io_destroy),
#endif 

#if defined(__NR_io_getevents)
    SYSCALL_ENTRY_ITEM (io_getevents),
#endif 

#if defined(__NR_io_submit)
    SYSCALL_ENTRY_ITEM (io_submit),
#endif 

#if defined(__NR_io_cancel)
    SYSCALL_ENTRY_ITEM (io_cancel),
#endif 

#if defined(__NR_fadvise64)
    SYSCALL_ENTRY_ITEM (fadvise64),
#endif 

#if defined(__NR_exit_group)
    SYSCALL_ENTRY_ITEM (exit_group),
#endif 

#if defined(__NR_lookup_dcookie)
    SYSCALL_ENTRY_ITEM (lookup_dcookie),
#endif 

#if defined(__NR_epoll_create)
    SYSCALL_ENTRY_ITEM (epoll_create),
#endif 

#if defined(__NR_epoll_ctl)
    SYSCALL_ENTRY_ITEM (epoll_ctl),
#endif 

#if defined(__NR_epoll_wait)
    SYSCALL_ENTRY_ITEM (epoll_wait),
#endif 

#if defined(__NR_remap_file_pages)
    SYSCALL_ENTRY_ITEM (remap_file_pages),
#endif 

#if defined(__NR_set_tid_address)
    SYSCALL_ENTRY_ITEM (set_tid_address),
#endif 

#if defined(__NR_timer_create)
    SYSCALL_ENTRY_ITEM (timer_create),
#endif 

#if defined(__NR_timer_settime32)
    SYSCALL_ENTRY_ITEM (timer_settime32),
#endif 

#if defined(__NR_timer_settime)
    SYSCALL_ENTRY_ITEM (timer_settime),
#endif 

#if defined(__NR_timer_gettime32)
    SYSCALL_ENTRY_ITEM (timer_gettime32),
#endif 

#if defined(__NR_timer_gettime)
    SYSCALL_ENTRY_ITEM (timer_gettime),
#endif 

#if defined(__NR_timer_getoverrun)
    SYSCALL_ENTRY_ITEM (timer_getoverrun),
#endif 

#if defined(__NR_timer_delete)
    SYSCALL_ENTRY_ITEM (timer_delete),
#endif 

#if defined(__NR_clock_settime)
    SYSCALL_ENTRY_ITEM (clock_settime),
#endif 
#if defined(__NR_clock_gettime)
    SYSCALL_ENTRY_ITEM (clock_gettime),
#endif 

#if defined(__NR_clock_settime32)
    SYSCALL_ENTRY_ITEM (clock_settime32),
#endif 

#if defined(__NR_clock_gettime32)
    SYSCALL_ENTRY_ITEM (clock_gettime32),
#endif 

#if defined(__NR_clock_getres_time32)
    SYSCALL_ENTRY_ITEM (clock_getres_time32),
#endif 

#if defined(__NR_clock_nanosleep_time32)
    SYSCALL_ENTRY_ITEM (clock_nanosleep_time32),
#endif 

#if defined(__NR_statfs64)
    SYSCALL_ENTRY_ITEM (statfs64),
#endif 

#if defined(__NR_fstatfs64)
    SYSCALL_ENTRY_ITEM (fstatfs64),
#endif 

#if defined(__NR_tgkill)
    SYSCALL_ENTRY_ITEM (tgkill),
#endif 

#if defined(__NR_utimes)
    SYSCALL_ENTRY_ITEM (utimes),
#endif 

#if defined(__NR_fadvise64_64)
    SYSCALL_ENTRY_ITEM (fadvise64_64),
#endif 

#if defined(__NR_vserver)
    SYSCALL_ENTRY_ITEM (vserver),
#endif 

#if defined(__NR_mbind)
    SYSCALL_ENTRY_ITEM (mbind),
#endif 

#if defined(__NR_get_mempolicy)
    SYSCALL_ENTRY_ITEM (get_mempolicy),
#endif 

#if defined(__NR_set_mempolicy)
    SYSCALL_ENTRY_ITEM (set_mempolicy),
#endif 

#if defined(__NR_mq_open)
    SYSCALL_ENTRY_ITEM (mq_open),
#endif 

#if defined(__NR_mq_unlink)
    SYSCALL_ENTRY_ITEM (mq_unlink),
#endif 

#if defined(__NR_mq_timedsend)
    SYSCALL_ENTRY_ITEM (mq_timedsend),
#endif 

#if defined(__NR_mq_timedreceive)
    SYSCALL_ENTRY_ITEM (mq_timedreceive),
#endif 

#if defined(__NR_mq_notify)
    SYSCALL_ENTRY_ITEM (mq_notify),
#endif 

#if defined(__NR_mq_getsetattr)
    SYSCALL_ENTRY_ITEM (mq_getsetattr),
#endif 

#if defined(__NR_kexec_load)
    SYSCALL_ENTRY_ITEM (kexec_load),
#endif 

#if defined(__NR_waitid)
    SYSCALL_ENTRY_ITEM (waitid),
#endif 

#if defined(__NR_add_key)
    SYSCALL_ENTRY_ITEM (add_key),
#endif 

#if defined(__NR_request_key)
    SYSCALL_ENTRY_ITEM (request_key),
#endif 

#if defined(__NR_keyctl)
    SYSCALL_ENTRY_ITEM (keyctl),
#endif 

#if defined(__NR_ioprio_set)
    SYSCALL_ENTRY_ITEM (ioprio_set),
#endif 

#if defined(__NR_ioprio_get)
    SYSCALL_ENTRY_ITEM (ioprio_get),
#endif 

#if defined(__NR_inotify_init)
    SYSCALL_ENTRY_ITEM (inotify_init),
#endif 

#if defined(__NR_inotify_add_watch)
    SYSCALL_ENTRY_ITEM (inotify_add_watch),
#endif 

#if defined(__NR_inotify_rm_watch)
    SYSCALL_ENTRY_ITEM (inotify_rm_watch),
#endif 

#if defined(__NR_migrate_pages)
    SYSCALL_ENTRY_ITEM (migrate_pages),
#endif 

#if defined(__NR_openat)
    SYSCALL_ENTRY_ITEM (openat),
#endif 

#if defined(__NR_mkdirat)
    SYSCALL_ENTRY_ITEM (mkdirat),
#endif 

#if defined(__NR_mknodat)
    SYSCALL_ENTRY_ITEM (mknodat),
#endif 

#if defined(__NR_fchownat)
    SYSCALL_ENTRY_ITEM (fchownat),
#endif 

#if defined(__NR_futimesat)
    SYSCALL_ENTRY_ITEM (futimesat),
#endif 

#if defined(__NR_fstatat64)
    SYSCALL_ENTRY_ITEM (fstatat64),
#endif 

#if defined(__NR_newfstatat)
    SYSCALL_ENTRY_ITEM (newfstatat),
#endif 

#if defined(__NR_unlinkat)
    SYSCALL_ENTRY_ITEM (unlinkat),
#endif 

#if defined(__NR_renameat)
    SYSCALL_ENTRY_ITEM (renameat),
#endif 

#if defined(__NR_linkat)
    SYSCALL_ENTRY_ITEM (linkat),
#endif 

#if defined(__NR_symlinkat)
    SYSCALL_ENTRY_ITEM (symlinkat),
#endif 

#if defined(__NR_readlinkat)
    SYSCALL_ENTRY_ITEM (readlinkat),
#endif 

#if defined(__NR_fchmodat)
    SYSCALL_ENTRY_ITEM (fchmodat),
#endif 

#if defined(__NR_faccessat)
    SYSCALL_ENTRY_ITEM (faccessat),
#endif 

#if defined(__NR_pselect6)
    SYSCALL_ENTRY_ITEM (pselect6),
#endif 

#if defined(__NR_ppoll)
    SYSCALL_ENTRY_ITEM (ppoll),
#endif 

#if defined(__NR_unshare)
    SYSCALL_ENTRY_ITEM (unshare),
#endif 

#if defined(__NR_set_robust_list)
    SYSCALL_ENTRY_ITEM (set_robust_list),
#endif 

#if defined(__NR_get_robust_list)
    SYSCALL_ENTRY_ITEM (get_robust_list),
#endif 

#if defined(__NR_splice)
    SYSCALL_ENTRY_ITEM (splice),
#endif 

#if defined(__NR_sync_file_range)
    SYSCALL_ENTRY_ITEM (sync_file_range),
#endif 

#if defined(__NR_tee)
    SYSCALL_ENTRY_ITEM (tee),
#endif 

#if defined(__NR_vmsplice)
    SYSCALL_ENTRY_ITEM (vmsplice),
#endif 

#if defined(__NR_move_pages)
    SYSCALL_ENTRY_ITEM (move_pages),
#endif 

#if defined(__NR_getcpu)
    SYSCALL_ENTRY_ITEM (getcpu),
#endif 

#if defined(__NR_epoll_pwait)
    SYSCALL_ENTRY_ITEM (epoll_pwait),
#endif 

#if defined(__NR_utimensat)
    SYSCALL_ENTRY_ITEM (utimensat),
#endif 

#if defined(__NR_signalfd)
    SYSCALL_ENTRY_ITEM (signalfd),
#endif 

#if defined(__NR_timerfd_create)
    SYSCALL_ENTRY_ITEM (timerfd_create),
#endif 

#if defined(__NR_eventfd)
    SYSCALL_ENTRY_ITEM (eventfd),
#endif 

#if defined(__NR_fallocate)
    SYSCALL_ENTRY_ITEM (fallocate),
#endif 

#if defined(__NR_timerfd_settime32)
    SYSCALL_ENTRY_ITEM (timerfd_settime32),
#endif 

#if defined(__NR_timerfd_gettime32)
    SYSCALL_ENTRY_ITEM (timerfd_gettime32),
#endif 

#if defined(__NR_signalfd4)
    SYSCALL_ENTRY_ITEM (signalfd4),
#endif 

#if defined(__NR_eventfd2)
    SYSCALL_ENTRY_ITEM (eventfd2),
#endif 

#if defined(__NR_epoll_create1)
    SYSCALL_ENTRY_ITEM (epoll_create1),
#endif 

#if defined(__NR_dup3)
    SYSCALL_ENTRY_ITEM (dup3),
#endif 

#if defined(__NR_pipe2)
    SYSCALL_ENTRY_ITEM (pipe2),
#endif 

#if defined(__NR_inotify_init1)
    SYSCALL_ENTRY_ITEM (inotify_init1),
#endif 

#if defined(__NR_preadv)
    SYSCALL_ENTRY_ITEM (preadv),
#endif 

#if defined(__NR_pwritev)
    SYSCALL_ENTRY_ITEM (pwritev),
#endif 

#if defined(__NR_rt_tgsigqueueinfo)
    SYSCALL_ENTRY_ITEM (rt_tgsigqueueinfo),
#endif 

#if defined(__NR_perf_event_open)
    SYSCALL_ENTRY_ITEM (perf_event_open),
#endif 

#if defined(__NR_recvmmsg)
    SYSCALL_ENTRY_ITEM (recvmmsg),
#endif 

#if defined(__NR_fanotify_init)
    SYSCALL_ENTRY_ITEM (fanotify_init),
#endif 

#if defined(__NR_fanotify_mark)
    SYSCALL_ENTRY_ITEM (fanotify_mark),
#endif 

#if defined(__NR_prlimit64)
    SYSCALL_ENTRY_ITEM (prlimit64),
#endif 

#if defined(__NR_name_to_handle_at)
    SYSCALL_ENTRY_ITEM (name_to_handle_at),
#endif 

#if defined(__NR_open_by_handle_at)
    SYSCALL_ENTRY_ITEM (open_by_handle_at),
#endif 

#if defined(__NR_clock_adjtime)
    SYSCALL_ENTRY_ITEM (clock_adjtime),
#endif 

#if defined(__NR_syncfs)
    SYSCALL_ENTRY_ITEM (syncfs),
#endif 

#if defined(__NR_sendmmsg)
    SYSCALL_ENTRY_ITEM (sendmmsg),
#endif 

#if defined(__NR_setns)
    SYSCALL_ENTRY_ITEM (setns),
#endif 

#if defined(__NR_process_vm_readv)
    SYSCALL_ENTRY_ITEM (process_vm_readv),
#endif 

#if defined(__NR_process_vm_writev)
    SYSCALL_ENTRY_ITEM (process_vm_writev),
#endif 

#if defined(__NR_kcmp)
    SYSCALL_ENTRY_ITEM (kcmp),
#endif 

#if defined(__NR_sched_setattr)
    SYSCALL_ENTRY_ITEM (sched_setattr),
#endif 

#if defined(__NR_sched_getattr)
    SYSCALL_ENTRY_ITEM (sched_getattr),
#endif 

#if defined(__NR_renameat2)
    SYSCALL_ENTRY_ITEM (renameat2),
#endif 

#if defined(__NR_seccomp)
    SYSCALL_ENTRY_ITEM (seccomp),
#endif 

#if defined(__NR_getrandom)
    SYSCALL_ENTRY_ITEM (getrandom),
#endif 

#if defined(__NR_memfd_create)
    SYSCALL_ENTRY_ITEM (memfd_create),
#endif 

#if defined(__NR_bpf)
    SYSCALL_ENTRY_ITEM (bpf),
#endif 

#if defined(__NR_execveat)
    SYSCALL_ENTRY_ITEM (execveat),
#endif 

#if defined(__NR_socket)
    SYSCALL_ENTRY_ITEM (socket),
#endif 

#if defined(__NR_socketpair)
    SYSCALL_ENTRY_ITEM (socketpair),
#endif 

#if defined(__NR_bind)
    SYSCALL_ENTRY_ITEM (bind),
#endif 

#if defined(__NR_connect)
    SYSCALL_ENTRY_ITEM (connect),
#endif 

#if defined(__NR_listen)
    SYSCALL_ENTRY_ITEM (listen),
#endif 

#if defined(__NR_accept)
    SYSCALL_ENTRY_ITEM (accept),
#endif 

#if defined(__NR_accept4)
    SYSCALL_ENTRY_ITEM (accept4),
#endif 

#if defined(__NR_getsockopt)
    SYSCALL_ENTRY_ITEM (getsockopt),
#endif 

#if defined(__NR_setsockopt)
    SYSCALL_ENTRY_ITEM (setsockopt),
#endif 

#if defined(__NR_getsockname)
    SYSCALL_ENTRY_ITEM (getsockname),
#endif 

#if defined(__NR_getpeername)
    SYSCALL_ENTRY_ITEM (getpeername),
#endif 

#if defined(__NR_sendto)
    SYSCALL_ENTRY_ITEM (sendto),
#endif 

#if defined(__NR_sendmsg)
    SYSCALL_ENTRY_ITEM (sendmsg),
#endif 

#if defined(__NR_recvfrom)
    SYSCALL_ENTRY_ITEM (recvfrom),
#endif 

#if defined(__NR_recvmsg)
    SYSCALL_ENTRY_ITEM (recvmsg),
#endif 

#if defined(__NR_shutdown)
    SYSCALL_ENTRY_ITEM (shutdown),
#endif 

#if defined(__NR_userfaultfd)
    SYSCALL_ENTRY_ITEM (userfaultfd),
#endif 

#if defined(__NR_membarrier)
    SYSCALL_ENTRY_ITEM (membarrier),
#endif 

#if defined(__NR_mlock2)
    SYSCALL_ENTRY_ITEM (mlock2),
#endif 

#if defined(__NR_copy_file_range)
    SYSCALL_ENTRY_ITEM (copy_file_range),
#endif 

#if defined(__NR_preadv2)
    SYSCALL_ENTRY_ITEM (preadv2),
#endif 

#if defined(__NR_pwritev2)
    SYSCALL_ENTRY_ITEM (pwritev2),
#endif 

#if defined(__NR_pkey_mprotect)
    SYSCALL_ENTRY_ITEM (pkey_mprotect),
#endif 

#if defined(__NR_pkey_alloc)
    SYSCALL_ENTRY_ITEM (pkey_alloc),
#endif 

#if defined(__NR_pkey_free)
    SYSCALL_ENTRY_ITEM (pkey_free),
#endif 

#if defined(__NR_statx)
    SYSCALL_ENTRY_ITEM (statx),
#endif 

#if defined(__NR_arch_prctl)
    SYSCALL_ENTRY_ITEM (arch_prctl),
#endif 

#if defined(__NR_io_pgetevents)
    SYSCALL_ENTRY_ITEM (io_pgetevents),
#endif 

#if defined(__NR_rseq)
    SYSCALL_ENTRY_ITEM (rseq),
#endif 

#if defined(__NR_semget)
    SYSCALL_ENTRY_ITEM (semget),
#endif 

#if defined(__NR_semctl)
    SYSCALL_ENTRY_ITEM (semctl),
#endif 

#if defined(__NR_shmget)
    SYSCALL_ENTRY_ITEM (shmget),
#endif 

#if defined(__NR_shmctl)
    SYSCALL_ENTRY_ITEM (shmctl),
#endif 

#if defined(__NR_shmat)
    SYSCALL_ENTRY_ITEM (shmat),
#endif 

#if defined(__NR_shmdt)
    SYSCALL_ENTRY_ITEM (shmdt),
#endif 

#if defined(__NR_msgget)
    SYSCALL_ENTRY_ITEM (msgget),
#endif 

#if defined(__NR_msgsnd)
    SYSCALL_ENTRY_ITEM (msgsnd),
#endif 

#if defined(__NR_msgrcv)
    SYSCALL_ENTRY_ITEM (msgrcv),
#endif 

#if defined(__NR_msgctl)
    SYSCALL_ENTRY_ITEM (msgctl),
#endif 

#if defined(__NR_clock_gettime64)
    SYSCALL_ENTRY_ITEM (clock_gettime64),
#endif 

#if defined(__NR_clock_settime64)
    SYSCALL_ENTRY_ITEM (clock_settime64),
#endif 

#if defined(__NR_clock_adjtime64)
    SYSCALL_ENTRY_ITEM (clock_adjtime64),
#endif 

#if defined(__NR_clock_getres)
    SYSCALL_ENTRY_ITEM (clock_getres),
#endif 

#if defined(__NR_clock_getres_time64)
    SYSCALL_ENTRY_ITEM (clock_getres_time64),
#endif 

#if defined(__NR_clock_nanosleep)
    SYSCALL_ENTRY_ITEM (clock_nanosleep),
#endif 

#if defined(__NR_clock_nanosleep_time64)
    SYSCALL_ENTRY_ITEM (clock_nanosleep_time64),
#endif 

#if defined(__NR_timer_gettime64)
    SYSCALL_ENTRY_ITEM (timer_gettime64),
#endif 

#if defined(__NR_timer_settime64)
    SYSCALL_ENTRY_ITEM (timer_settime64),
#endif 

#if defined(__NR_timerfd_gettime64)
    SYSCALL_ENTRY_ITEM (timerfd_gettime64),
#endif 

#if defined(__NR_timerfd_settime64)
    SYSCALL_ENTRY_ITEM (timerfd_settime64),
#endif 

#if defined(__NR_utimensat_time64)
    SYSCALL_ENTRY_ITEM (utimensat_time64),
#endif 

#if defined(__NR_pselect6_time64)
    SYSCALL_ENTRY_ITEM (pselect6_time64),
#endif 

#if defined(__NR_ppoll_time64)
    SYSCALL_ENTRY_ITEM (ppoll_time64),
#endif 

#if defined(__NR_io_pgetevents_time64)
    SYSCALL_ENTRY_ITEM (io_pgetevents_time64),
#endif 

#if defined(__NR_recvmmsg_time64)
    SYSCALL_ENTRY_ITEM (recvmmsg_time64),
#endif 

#if defined(__NR_mq_timedsend_time64)
    SYSCALL_ENTRY_ITEM (mq_timedsend_time64),
#endif 

#if defined(__NR_mq_timedreceive_time64)
    SYSCALL_ENTRY_ITEM (mq_timedreceive_time64),
#endif 

#if defined(__NR_semtimedop_time64)
    SYSCALL_ENTRY_ITEM (semtimedop_time64),
#endif 

#if defined(__NR_rt_sigtimedwait_time64)
    SYSCALL_ENTRY_ITEM (rt_sigtimedwait_time64),
#endif 

#if defined(__NR_futex_time64)
    SYSCALL_ENTRY_ITEM (futex_time64),
#endif 

#if defined(__NR_sched_rr_get_interval_time64)
    SYSCALL_ENTRY_ITEM (sched_rr_get_interval_time64),
#endif 

#if defined(__NR_pidfd_send_signal)
    SYSCALL_ENTRY_ITEM (pidfd_send_signal),
#endif 

#if defined(__NR_io_uring_setup)
    SYSCALL_ENTRY_ITEM (io_uring_setup),
#endif 

#if defined(__NR_io_uring_enter)
    SYSCALL_ENTRY_ITEM (io_uring_enter),
#endif 

#if defined(__NR_io_uring_register)
    SYSCALL_ENTRY_ITEM (io_uring_register),
#endif 

#if defined(__NR_open_tree)
    SYSCALL_ENTRY_ITEM (open_tree),
#endif 

#if defined(__NR_move_mount)
    SYSCALL_ENTRY_ITEM (move_mount),
#endif 

#if defined(__NR_fsopen)
    SYSCALL_ENTRY_ITEM (fsopen),
#endif 

#if defined(__NR_fsconfig)
    SYSCALL_ENTRY_ITEM (fsconfig),
#endif 

#if defined(__NR_fsmount)
    SYSCALL_ENTRY_ITEM (fsmount),
#endif 

#if defined(__NR_fspick)
    SYSCALL_ENTRY_ITEM (fspick),
#endif 

#if defined(__NR_pidfd_open)
    SYSCALL_ENTRY_ITEM (pidfd_open),
#endif 

#if defined(__NR_clone3)
    SYSCALL_ENTRY_ITEM (clone3),
#endif 

#if defined(__NR_close_range)
    SYSCALL_ENTRY_ITEM (close_range),
#endif 

#if defined(__NR_openat2)
    SYSCALL_ENTRY_ITEM (openat2),
#endif 

#if defined(__NR_pidfd_getfd)
    SYSCALL_ENTRY_ITEM (pidfd_getfd),
#endif 

#if defined(__NR_faccessat2)
    SYSCALL_ENTRY_ITEM (faccessat2),
#endif 

#if defined(__NR_process_madvise)
    SYSCALL_ENTRY_ITEM (process_madvise),
#endif 

#if defined(__NR_epoll_pwait2)
    SYSCALL_ENTRY_ITEM (epoll_pwait2),
#endif 

#if defined(__NR_mount_setattr)
    SYSCALL_ENTRY_ITEM (mount_setattr),
#endif 

#if defined(__NR_landlock_create_ruleset)
    SYSCALL_ENTRY_ITEM (landlock_create_ruleset),
#endif 

#if defined(__NR_landlock_add_rule)
    SYSCALL_ENTRY_ITEM (landlock_add_rule),
#endif 

    SYSCALL_ENTRY_ITEM (landlock_restrict_self)
};

void *syscall_extent_table[] = {
    period_thread_create,                  
    period_thread_wait_period,             
    syscall_period_thread_active,          
    syscall_period_sched_group_create,     
    syscall_period_sched_group_add,        
    syscall_period_thread_get_unstart_num, 
    syscall_pthread_getperiodcount,        
    syscall_pthread_getperiodexecutetime,  
    syscall_pthread_getperiodcompletetime, 
    syscall_pthread_getperiodexpireinfo    
};

bool is_extent_syscall_num (unsigned int syscall_num)
{
    if (syscall_num >= CONFIG_EXTENT_SYSCALL_NUM_START
        && syscall_num <= CONFIG_EXTENT_SYSCALL_NUM_END)
    {
        return true;
    }
    else
    {
        return false;
    }
}

long syscall_dispatch (unsigned int syscall_num, unsigned long arg0, unsigned long arg1,
            unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
    unsigned long args[6];

    trace_syscall_args_init(args, arg0, arg1, arg2, arg3, arg4, arg5);

    trace_syscall(syscall_num, 1, args);

    long ret = ((syscall_func)(syscall_table[syscall_num]))(arg0, arg1, arg2, arg3,
                                    arg4, arg5);

    trace_syscall(syscall_num, 0, &ret);

    return ret;
}
