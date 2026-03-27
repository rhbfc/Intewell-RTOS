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
#include <errno.h>

#if defined(__NR_restart_syscall)
DEFINE_SYSCALL_STUB(restart_syscall, (void))
#endif /* defined(__NR_restart_syscall) */

#if defined(__NR_exit)
DEFINE_SYSCALL_STUB(exit, (int error_code))
#endif /* defined(__NR_exit) */

#if defined(__NR_fork)
DEFINE_SYSCALL_STUB(fork, (void))
#endif /* defined(__NR_fork) */

#if defined(__NR_read)
DEFINE_SYSCALL_STUB(read, (int fd, char __user *buf, size_t count))
#endif /* defined(__NR_read) */

#if defined(__NR_write)
DEFINE_SYSCALL_STUB(write, (int fd, const char __user *buf, size_t count))
#endif /* defined(__NR_write) */

#if defined(__NR_open)
DEFINE_SYSCALL_STUB(open, (const char __user *filename, int flags, mode_t mode))
#endif /* defined(__NR_open) */

#if defined(__NR_close)
DEFINE_SYSCALL_STUB(close, (int fd))
#endif /* defined(__NR_close) */

#if defined(__NR_waitpid)
DEFINE_SYSCALL_STUB(waitpid, (pid_t pid, int __user *stat_addr, int options))
#endif /* defined(__NR_waitpid) */

#if defined(__NR_creat)
DEFINE_SYSCALL_STUB(creat, (const char __user *pathname, mode_t mode))
#endif /* defined(__NR_creat) */

#if defined(__NR_link)
DEFINE_SYSCALL_STUB(link, (const char __user *oldname, const char __user *newname))
#endif /* defined(__NR_link) */

#if defined(__NR_unlink)
DEFINE_SYSCALL_STUB(unlink, (const char __user *pathname))
#endif /* defined(__NR_unlink) */

#if defined(__NR_execve)
DEFINE_SYSCALL_STUB(execve, (const char __user *filename, const char __user *const __user *argv,
                             const char __user *const __user *envp))
#endif /* defined(__NR_execve) */

#if defined(__NR_chdir)
DEFINE_SYSCALL_STUB(chdir, (const char __user *filename))
#endif /* defined(__NR_chdir) */

#if defined(__NR_time)
DEFINE_SYSCALL_STUB(time, (time_t __user * tloc))
#endif /* defined(__NR_time) */

#if defined(__NR_mknod)
DEFINE_SYSCALL_STUB(mknod, (const char __user *filename, mode_t mode, unsigned dev))
#endif /* defined(__NR_mknod) */

#if defined(__NR_chmod)
DEFINE_SYSCALL_STUB(chmod, (const char __user *filename, mode_t mode))
#endif /* defined(__NR_chmod) */

#if defined(__NR_lchown)
DEFINE_SYSCALL_STUB(lchown, (const char __user *filename, uid_t user, gid_t group))
#endif /* defined(__NR_lchown) */

#if defined(__NR_break)
DEFINE_SYSCALL_STUB(break, (void))
#endif /* defined(__NR_break) */
       /* linux not support */
#if defined(__NR_oldstat)
DEFINE_SYSCALL_STUB(oldstat, (void))
#endif /* defined(__NR_oldstat) */
       /* linux not support */
#if defined(__NR_lseek)
DEFINE_SYSCALL_STUB(lseek, (int fd, off_t offset, unsigned int whence))
#endif /* defined(__NR_lseek) */

#if defined(__NR_getpid)
DEFINE_SYSCALL_STUB(getpid, (void))
#endif /* defined(__NR_getpid) */

#if defined(__NR_mount)
DEFINE_SYSCALL_STUB(mount, (char __user *dev_name, char __user *dir_name, char __user *type,
                            unsigned long flags, void __user *data))
#endif /* defined(__NR_mount) */

#if defined(__NR_umount)
DEFINE_SYSCALL_STUB(umount, (char __user *name))
#endif /* defined(__NR_umount) */

#if defined(__NR_setuid)
DEFINE_SYSCALL_STUB(setuid, (uid_t uid))
#endif /* defined(__NR_setuid) */

#if defined(__NR_getuid)
DEFINE_SYSCALL_STUB(getuid, (void))
#endif /* defined(__NR_getuid) */

#if defined(__NR_stime)
DEFINE_SYSCALL_STUB(stime, (time_t __user * tptr))
#endif /* defined(__NR_stime) */

#if defined(__NR_alarm)
DEFINE_SYSCALL_STUB(alarm, (unsigned int seconds))
#endif /* defined(__NR_alarm) */

#if defined(__NR_oldfstat)
DEFINE_SYSCALL_STUB(oldfstat, (void))
#endif /* defined(__NR_oldfstat) */
       /* linux not support */
#if defined(__NR_pause)
DEFINE_SYSCALL_STUB(pause, (void))
#endif /* defined(__NR_pause) */

#if defined(__NR_utime)
DEFINE_SYSCALL_STUB(utime, (char __user *filename, struct utimbuf __user *times))
#endif /* defined(__NR_utime) */

#if defined(__NR_stty)
DEFINE_SYSCALL_STUB(stty, (void))
#endif /* defined(__NR_stty) */
       /* linux not support */
#if defined(__NR_gtty)
DEFINE_SYSCALL_STUB(gtty, (void))
#endif /* defined(__NR_gtty) */
       /* linux not support */
#if defined(__NR_access)
DEFINE_SYSCALL_STUB(access, (const char __user *filename, int mode))
#endif /* defined(__NR_access) */

#if defined(__NR_nice)
DEFINE_SYSCALL_STUB(nice, (int increment))
#endif /* defined(__NR_nice) */

#if defined(__NR_ftime)
DEFINE_SYSCALL_STUB(ftime, (void))
#endif /* defined(__NR_ftime) */
       /* linux not support */
#if defined(__NR_sync)
DEFINE_SYSCALL_STUB(sync, (void))
#endif /* defined(__NR_sync) */

#if defined(__NR_kill)
DEFINE_SYSCALL_STUB(kill, (pid_t pid, int sig))
#endif /* defined(__NR_kill) */

#if defined(__NR_rename)
DEFINE_SYSCALL_STUB(rename, (const char __user *oldname, const char __user *newname))
#endif /* defined(__NR_rename) */

#if defined(__NR_mkdir)
DEFINE_SYSCALL_STUB(mkdir, (const char __user *pathname, mode_t mode))
#endif /* defined(__NR_mkdir) */

#if defined(__NR_rmdir)
DEFINE_SYSCALL_STUB(rmdir, (const char __user *pathname))
#endif /* defined(__NR_rmdir) */

#if defined(__NR_dup)
DEFINE_SYSCALL_STUB(dup, (unsigned int fildes))
#endif /* defined(__NR_dup) */

#if defined(__NR_pipe)
DEFINE_SYSCALL_STUB(pipe, (int __user *fildes))
#endif /* defined(__NR_pipe) */

#if defined(__NR_times)
DEFINE_SYSCALL_STUB(times, (struct tms __user * tbuf))
#endif /* defined(__NR_times) */

#if defined(__NR_prof)
DEFINE_SYSCALL_STUB(profil, (void))
#endif /* defined(__NR_prof) */
       /* linux not support */
#if defined(__NR_brk)
DEFINE_SYSCALL_STUB(brk, (unsigned long brk))
#endif /* defined(__NR_brk) */

#if defined(__NR_setgid)
DEFINE_SYSCALL_STUB(setgid, (gid_t gid))
#endif /* defined(__NR_setgid) */

#if defined(__NR_getgid)
DEFINE_SYSCALL_STUB(getgid, (void))
#endif /* defined(__NR_getgid) */

#if defined(__NR_signal)
DEFINE_SYSCALL_STUB(signal, (int sig, sighandler_t handler))
#endif /* defined(__NR_signal) */

#if defined(__NR_geteuid)
DEFINE_SYSCALL_STUB(geteuid, (void))
#endif /* defined(__NR_geteuid) */

#if defined(__NR_getegid)
DEFINE_SYSCALL_STUB(getegid, (void))
#endif /* defined(__NR_getegid) */

#if defined(__NR_acct)
DEFINE_SYSCALL_STUB(acct, (const char __user *name))
#endif /* defined(__NR_acct) */

#if defined(__NR_umount2)
DEFINE_SYSCALL_STUB(umount2, (char __user *name, int flags))
#endif /* defined(__NR_umount2) */
       /* linux not support */
#if defined(__NR_lock)
DEFINE_SYSCALL_STUB(lock, (void))
#endif /* defined(__NR_lock) */
       /* linux not support */
#if defined(__NR_ioctl)
DEFINE_SYSCALL_STUB(ioctl, (int fd, unsigned int cmd, unsigned long arg))
#endif /* defined(__NR_ioctl) */

#if defined(__NR_fcntl)
DEFINE_SYSCALL_STUB(fcntl, (int fd, unsigned int cmd, unsigned long arg))
#endif /* defined(__NR_fcntl) */

#if defined(__NR_mpx)
DEFINE_SYSCALL_STUB(mpx, (void))
#endif /* defined(__NR_mpx) */
       /* linux not support */
#if defined(__NR_setpgid)
DEFINE_SYSCALL_STUB(setpgid, (pid_t pid, pid_t pgid))
#endif /* defined(__NR_setpgid) */

#if defined(__NR_ulimit)
DEFINE_SYSCALL_STUB(ulimit, (void))
#endif /* defined(__NR_ulimit) */
       /* linux not support */
#if defined(__NR_oldolduname)
DEFINE_SYSCALL_STUB(oldolduname, (void))
#endif /* defined(__NR_oldolduname) */
       /* linux not support */
#if defined(__NR_umask)
DEFINE_SYSCALL_STUB(umask, (int mask))
#endif /* defined(__NR_umask) */

#if defined(__NR_chroot)
DEFINE_SYSCALL_STUB(chroot, (const char __user *filename))
#endif /* defined(__NR_chroot) */

#if defined(__NR_ustat)
DEFINE_SYSCALL_STUB(ustat, (void))
#endif /* defined(__NR_ustat) */
       /* musl not use */
#if defined(__NR_dup2)
DEFINE_SYSCALL_STUB(dup2, (unsigned int oldfd, unsigned int newfd))
#endif /* defined(__NR_dup2) */

#if defined(__NR_getppid)
DEFINE_SYSCALL_STUB(getppid, (void))
#endif /* defined(__NR_getppid) */

#if defined(__NR_getpgrp)
DEFINE_SYSCALL_STUB(getpgrp, (void))
#endif /* defined(__NR_getpgrp) */

#if defined(__NR_setsid)
DEFINE_SYSCALL_STUB(setsid, (void))
#endif /* defined(__NR_setsid) */

#if defined(__NR_sigaction)
DEFINE_SYSCALL_STUB(sigaction,
                    (int sig, const struct k_sigaction __user *act, struct k_sigaction __user *oact))
#endif /* defined(__NR_sigaction) */
/* linux drop */
#if defined(__NR_sgetmask)
DEFINE_SYSCALL_STUB(sgetmask, (void))
#endif /* defined(__NR_sgetmask) */

#if defined(__NR_ssetmask)
DEFINE_SYSCALL_STUB(ssetmask, (int newmask))
#endif /* defined(__NR_ssetmask) */

#if defined(__NR_setreuid)
DEFINE_SYSCALL_STUB(setreuid, (uid_t ruid, uid_t euid))
#endif /* defined(__NR_setreuid) */

#if defined(__NR_setregid)
DEFINE_SYSCALL_STUB(setregid, (gid_t rgid, gid_t egid))
#endif /* defined(__NR_setregid) */

#if defined(__NR_sigsuspend)
DEFINE_SYSCALL_STUB(kernel_signal_suspend, (sigset_t mask))
#endif /* defined(__NR_sigsuspend) */
/* linux drop */
#if defined(__NR_sigpending)
DEFINE_SYSCALL_STUB(sigpending, (sigset_t __user * set))
#endif /* defined(__NR_sigpending) */

#if defined(__NR_sethostname)
DEFINE_SYSCALL_STUB(sethostname, (char __user *name, int len))
#endif /* defined(__NR_sethostname) */

#if defined(__NR_setrlimit)
DEFINE_SYSCALL_STUB(setrlimit, (unsigned int resource, struct rlimit __user *rlim))
#endif /* defined(__NR_setrlimit) */

#if defined(__NR_getrlimit)
DEFINE_SYSCALL_STUB(getrlimit, (unsigned int resource, struct rlimit __user *rlim))
#endif /* defined(__NR_getrlimit) */

#if defined(__NR_getrusage)
DEFINE_SYSCALL_STUB(getrusage, (int who, struct rusage __user *ru))
#endif /* defined(__NR_getrusage) */

#if defined(__NR_gettimeofday_time32)
DEFINE_SYSCALL_STUB(gettimeofday_time32, (struct timeval __user * tp, struct timezone __user *tzp))
#endif /* defined(__NR_gettimeofday_time32) */

#if defined(__NR_gettimeofday)
DEFINE_SYSCALL_STUB(gettimeofday, (struct timeval __user * tp, struct timezone __user *tzp))
#endif /* defined(__NR_gettimeofday) */

#if defined(__NR_settimeofday_time32)
DEFINE_SYSCALL_STUB(settimeofday_time32,
                    (const struct __user timeval *tv, const struct __user timezone *tz))
#endif /* defined(__NR_settimeofday_time32) */

#if defined(__NR_settimeofday)
DEFINE_SYSCALL_STUB(settimeofday,
                    (const struct __user timeval *tv, const struct __user timezone *tz))
#endif /* defined(__NR_settimeofday) */

#if defined(__NR_getgroups)
DEFINE_SYSCALL_STUB(getgroups, (int gidsetsize, gid_t __user *grouplist))
#endif /* defined(__NR_getgroups) */

#if defined(__NR_setgroups)
DEFINE_SYSCALL_STUB(setgroups, (int gidsetsize, gid_t __user *grouplist))
#endif /* defined(__NR_setgroups) */

#if defined(__NR_select)
DEFINE_SYSCALL_STUB(select, (int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp,
                             struct timeval __user *tvp))
#endif /* defined(__NR_select) */

#if defined(__NR_symlink)
DEFINE_SYSCALL_STUB(symlink, (const char __user *old, const char __user *new))
#endif /* defined(__NR_symlink) */

#if defined(__NR_oldlstat)
DEFINE_SYSCALL_STUB(oldlstat, (void))
#endif /* defined(__NR_oldlstat) */
       /* linux not support */
#if defined(__NR_readlink)
DEFINE_SYSCALL_STUB(readlink, (const char __user *path, char __user *buf, int bufsiz))
#endif /* defined(__NR_readlink) */

#if defined(__NR_uselib)
DEFINE_SYSCALL_STUB(uselib, (const char __user *library))
#endif /* defined(__NR_uselib) */

#if defined(__NR_swapon)
DEFINE_SYSCALL_STUB(swapon, (const char __user *specialfile, int swap_flags))
#endif /* defined(__NR_swapon) */

#if defined(__NR_reboot)
DEFINE_SYSCALL_STUB(reboot, (int magic1, int magic2, unsigned int cmd, void __user *arg))
#endif /* defined(__NR_reboot) */

#if defined(__NR_readdir)
DEFINE_SYSCALL_STUB(readdir, (void))
#endif /* defined(__NR_readdir) */
       /* linux not support */
#if defined(__NR_mmap)
DEFINE_SYSCALL_STUB(mmap, (unsigned long addr, unsigned long len, unsigned long prot,
                           unsigned long flags, int fd, off_t off))
#endif /* defined(__NR_mmap) */

#if defined(__NR_munmap)
DEFINE_SYSCALL_STUB(munmap, (unsigned long addr, size_t len))
#endif /* defined(__NR_munmap) */

#if defined(__NR_truncate)
DEFINE_SYSCALL_STUB(truncate, (const char __user *path, long length))
#endif /* defined(__NR_truncate) */

#if defined(__NR_ftruncate)
DEFINE_SYSCALL_STUB(ftruncate, (int fd, loff_t length))
#endif /* defined(__NR_ftruncate) */

#if defined(__NR_fchmod)
DEFINE_SYSCALL_STUB(fchmod, (int fd, mode_t mode))
#endif /* defined(__NR_fchmod) */

#if defined(__NR_fchown)
DEFINE_SYSCALL_STUB(fchown, (int fd, uid_t user, gid_t group))
#endif /* defined(__NR_fchown) */

#if defined(__NR_getpriority)
DEFINE_SYSCALL_STUB(getpriority, (int which, int who))
#endif /* defined(__NR_getpriority) */

#if defined(__NR_setpriority)
DEFINE_SYSCALL_STUB(setpriority, (int which, int who, int niceval))
#endif /* defined(__NR_setpriority) */

#if defined(__NR_profil)
DEFINE_SYSCALL_STUB(profil, (void))
#endif /* defined(__NR_profil) */
       /* linux not support */
#if defined(__NR_statfs)
DEFINE_SYSCALL_STUB(statfs, (const char __user *path, struct statfs __user *buf))
#endif /* defined(__NR_statfs) */

#if defined(__NR_fstatfs)
DEFINE_SYSCALL_STUB(fstatfs, (int fd, struct statfs __user *buf))
#endif /* defined(__NR_fstatfs) */

#if defined(__NR_ioperm)
DEFINE_SYSCALL_STUB(ioperm, (unsigned long from, unsigned long num, int on))
#endif /* defined(__NR_ioperm) */

#if defined(__NR_socketcall)
DEFINE_SYSCALL_STUB(socketcall, (int call, unsigned long __user *args))
#endif /* defined(__NR_socketcall) */

#if defined(__NR_syslog)
DEFINE_SYSCALL_STUB(syslog, (int type, char __user *buf, int len))
#endif /* defined(__NR_syslog) */

#if defined(__NR_setitimer)
DEFINE_SYSCALL_STUB(setitimer,
                    (int which, struct itimerval __user *value, struct itimerval __user *ovalue))
#endif /* defined(__NR_setitimer) */

#if defined(__NR_getitimer)
DEFINE_SYSCALL_STUB(getitimer, (int which, struct itimerval __user *value))
#endif /* defined(__NR_getitimer) */

#if defined(__NR_stat)
DEFINE_SYSCALL_STUB(stat, (const char __user *filename, struct stat __user *statbuf))
#endif /* defined(__NR_stat) */

#if defined(__NR_lstat)
DEFINE_SYSCALL_STUB(lstat, (const char __user *filename, struct stat __user *statbuf))
#endif /* defined(__NR_lstat) */

#if defined(__NR_fstat)
DEFINE_SYSCALL_STUB(fstat, (int fd, struct stat __user *statbuf))
#endif /* defined(__NR_fstat) */

#if defined(__NR_olduname)
DEFINE_SYSCALL_STUB(olduname, (void))
#endif /* defined(__NR_olduname) */
       /* musl not use */
#if defined(__NR_iopl)
DEFINE_SYSCALL_STUB(iopl, (unsigned int level))
#endif /* defined(__NR_iopl) */

#if defined(__NR_vhangup)
DEFINE_SYSCALL_STUB(vhangup, (void))
#endif /* defined(__NR_vhangup) */

#if defined(__NR_idle)
DEFINE_SYSCALL_STUB(idle, (void))
#endif /* defined(__NR_idle) */

#if defined(__NR_vm86old)
DEFINE_SYSCALL_STUB(vm86old, (void))
#endif /* defined(__NR_vm86old) */
       /* musl not use */
#if defined(__NR_wait4)
DEFINE_SYSCALL_STUB(wait4,
                    (pid_t pid, int __user *stat_addr, int options, struct rusage __user *ru))
#endif /* defined(__NR_wait4) */

#if defined(__NR_swapoff)
DEFINE_SYSCALL_STUB(swapoff, (const char __user *specialfile))
#endif /* defined(__NR_swapoff) */

#if defined(__NR_sysinfo)
DEFINE_SYSCALL_STUB(sysinfo, (struct sysinfo __user * info))
#endif /* defined(__NR_sysinfo) */

#if defined(__NR_ipc)
DEFINE_SYSCALL_STUB(ipc, (unsigned int call, int first, unsigned long second, unsigned long third,
                          void __user *ptr, long fifth))
#endif /* defined(__NR_ipc) */

#if defined(__NR_fsync)
DEFINE_SYSCALL_STUB(fsync, (int fd))
#endif /* defined(__NR_fsync) */

#if defined(__NR_sigreturn)
DEFINE_SYSCALL_STUB(sigreturn, (void))
#endif /* defined(__NR_sigreturn) */

#ifdef CONFIG_CLONE_BACKWARDS
DEFINE_SYSCALL_STUB(clone,
                    (unsigned long clone_flags, unsigned long newsp, int __user *set_child_tid,
                     unsigned long tls, int __user *clear_child_tid))
#elif defined(CONFIG_CLONE_BACKWARDS2)
DEFINE_SYSCALL_STUB(clone,
                    (unsigned long newsp, unsigned long clone_flags, int __user *set_child_tid,
                     int __user *clear_child_tid, unsigned long tls))

#elif defined(CONFIG_CLONE_BACKWARDS3)
DEFINE_SYSCALL_STUB(clone,
                    (unsigned long clone_flags, unsigned long newsp, int stack_size,
                     int __user *set_child_tid, int __user *clear_child_tid, unsigned long tls))

#else
DEFINE_SYSCALL_STUB(clone,
                    (unsigned long clone_flags, unsigned long newsp, int __user *set_child_tid,
                     int __user *clear_child_tid, unsigned long tls))

#endif

#if defined(__NR_setdomainname)
DEFINE_SYSCALL_STUB(setdomainname, (char __user *name, int len))
#endif /* defined(__NR_setdomainname) */

#if defined(__NR_uname)
DEFINE_SYSCALL_STUB(uname, (struct utsname __user * name))
#endif /* defined(__NR_uname) */

#if defined(__NR_modify_ldt)
DEFINE_SYSCALL_STUB(modify_ldt, (int func, void __user *ptr, unsigned long bytecount))
#endif /* defined(__NR_modify_ldt) */

#if defined(__NR_adjtimex)
DEFINE_SYSCALL_STUB(adjtimex, (struct timex __user * txc_p))
#endif /* defined(__NR_adjtimex) */

#if defined(__NR_mprotect)
DEFINE_SYSCALL_STUB(mprotect, (unsigned long start, size_t len, unsigned long prot))
#endif /* defined(__NR_mprotect) */

#if defined(__NR_sigprocmask)
DEFINE_SYSCALL_STUB(sigprocmask, (int how, sigset_t __user *set, sigset_t __user *oset))
#endif /* defined(__NR_sigprocmask) */

#if defined(__NR_quotactl)
DEFINE_SYSCALL_STUB(quotactl, (int cmd, const char __user *special, int id, void __user *addr))
#endif /* defined(__NR_quotactl) */

#if defined(__NR_getpgid)
DEFINE_SYSCALL_STUB(getpgid, (pid_t pid))
#endif /* defined(__NR_getpgid) */

#if defined(__NR_fchdir)
DEFINE_SYSCALL_STUB(fchdir, (int fd))
#endif /* defined(__NR_fchdir) */

#if defined(__NR_bdflush)
DEFINE_SYSCALL_STUB(bdflush, (int func, long data))
#endif /* defined(__NR_bdflush) */

#if defined(__NR_sysfs)
DEFINE_SYSCALL_STUB(sysfs, (int option, unsigned long arg1, unsigned long arg2))
#endif /* defined(__NR_sysfs) */

#if defined(__NR_personality)
DEFINE_SYSCALL_STUB(personality, (unsigned int personality))
#endif /* defined(__NR_personality) */

#if defined(__NR_afs_syscall)
DEFINE_SYSCALL_STUB(afs_syscall, (void))
#endif /* defined(__NR_afs_syscall) */
       /* linux not support */
#if defined(__NR_setfsuid)
DEFINE_SYSCALL_STUB(setfsuid, (uid_t uid))
#endif /* defined(__NR_setfsuid) */

#if defined(__NR_setfsgid)
DEFINE_SYSCALL_STUB(setfsgid, (gid_t gid))
#endif /* defined(__NR_setfsgid) */

#if defined(__NR__llseek)
DEFINE_SYSCALL_STUB(_llseek, (int fd, unsigned long offset_high, unsigned long offset_low,
                              loff_t __user *result, unsigned int whence))
#endif /* defined(__NR__llseek) */

#if defined(__NR_getdents)
DEFINE_SYSCALL_STUB(getdents, (int fd, struct dirent __user *dirent, unsigned int count))
#endif /* defined(__NR_getdents) */

#if defined(__NR__newselect)
DEFINE_SYSCALL_STUB(_newselect, (int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp,
                                 struct timeval __user *tvp))
#endif /* defined(__NR__newselect) */
       /* linux not support */
#if defined(__NR_flock)
DEFINE_SYSCALL_STUB(flock, (int fd, unsigned int cmd))
#endif /* defined(__NR_flock) */

#if defined(__NR_msync)
DEFINE_SYSCALL_STUB(msync, (unsigned long start, size_t len, int flags))
#endif /* defined(__NR_msync) */

#if defined(__NR_readv)
DEFINE_SYSCALL_STUB(readv, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen))
#endif /* defined(__NR_readv) */

#if defined(__NR_writev)
DEFINE_SYSCALL_STUB(writev, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen))
#endif /* defined(__NR_writev) */

#if defined(__NR_getsid)
DEFINE_SYSCALL_STUB(getsid, (pid_t pid))
#endif /* defined(__NR_getsid) */

#if defined(__NR_fdatasync)
DEFINE_SYSCALL_STUB(fdatasync, (int fd))
#endif /* defined(__NR_fdatasync) */

#if defined(__NR__sysctl)
DEFINE_SYSCALL_STUB(_sysctl, (void))
#endif /* defined(__NR__sysctl) */
       /* musl not use */
#if defined(__NR_mlock)
DEFINE_SYSCALL_STUB(mlock, (unsigned long start, size_t len))
#endif /* defined(__NR_mlock) */

#if defined(__NR_munlock)
DEFINE_SYSCALL_STUB(munlock, (unsigned long start, size_t len))
#endif /* defined(__NR_munlock) */

#if defined(__NR_mlockall)
DEFINE_SYSCALL_STUB(mlockall, (int flags))
#endif /* defined(__NR_mlockall) */

#if defined(__NR_munlockall)
DEFINE_SYSCALL_STUB(munlockall, (void))
#endif /* defined(__NR_munlockall) */

#if defined(__NR_sched_setparam)
DEFINE_SYSCALL_STUB(sched_setparam, (pid_t pid, struct sched_param __user *param))
#endif /* defined(__NR_sched_setparam) */

#if defined(__NR_sched_getparam)
DEFINE_SYSCALL_STUB(sched_getparam, (pid_t pid, struct sched_param __user *param))
#endif /* defined(__NR_sched_getparam) */

#if defined(__NR_sched_setscheduler)
DEFINE_SYSCALL_STUB(sched_setscheduler, (pid_t pid, int policy, struct sched_param __user *param))
#endif /* defined(__NR_sched_setscheduler) */

#if defined(__NR_sched_getscheduler)
DEFINE_SYSCALL_STUB(sched_getscheduler, (pid_t pid))
#endif /* defined(__NR_sched_getscheduler) */

#if defined(__NR_sched_yield)
DEFINE_SYSCALL_STUB(sched_yield, (void))
#endif /* defined(__NR_sched_yield) */

#if defined(__NR_sched_get_priority_max)
DEFINE_SYSCALL_STUB(sched_get_priority_max, (int policy))
#endif /* defined(__NR_sched_get_priority_max) */

#if defined(__NR_sched_get_priority_min)
DEFINE_SYSCALL_STUB(sched_get_priority_min, (int policy))
#endif /* defined(__NR_sched_get_priority_min) */

#if defined(__NR_sched_rr_get_interval)
DEFINE_SYSCALL_STUB(sched_rr_get_interval, (pid_t pid, struct timespec __user *interval))
#endif /* defined(__NR_sched_rr_get_interval) */

#if defined(__NR_nanosleep)
DEFINE_SYSCALL_STUB(nanosleep, (long __user *rqtp, long __user *rmtp))
#endif /* defined(__NR_nanosleep) */

#if defined(__NR_mremap)
DEFINE_SYSCALL_STUB(mremap, (unsigned long addr, unsigned long old_len, unsigned long new_len,
                             unsigned long flags, unsigned long new_addr))
#endif /* defined(__NR_mremap) */

#if defined(__NR_setresuid)
DEFINE_SYSCALL_STUB(setresuid, (uid_t ruid, uid_t euid, uid_t suid))
#endif /* defined(__NR_setresuid) */

#if defined(__NR_getresuid)
DEFINE_SYSCALL_STUB(getresuid, (uid_t __user * ruid, uid_t __user *euid, uid_t __user *suid))
#endif /* defined(__NR_getresuid) */

#if defined(__NR_vm86)
DEFINE_SYSCALL_STUB(vm86, (unsigned long cmd, unsigned long arg))
#endif /* defined(__NR_vm86) */

#if defined(__NR_query_module)
DEFINE_SYSCALL_STUB(query_module, (void))
#endif /* defined(__NR_query_module) */
       /* linux not support */
#if defined(__NR_poll)
DEFINE_SYSCALL_STUB(poll, (struct pollfd __user * ufds, unsigned int nfds, int timeout))
#endif /* defined(__NR_poll) */

#if defined(__NR_nfsservctl)
DEFINE_SYSCALL_STUB(nfsservctl, (void))
#endif /* defined(__NR_nfsservctl) */
       /* linux not support */
#if defined(__NR_setresgid)
DEFINE_SYSCALL_STUB(setresgid, (gid_t rgid, gid_t egid, gid_t sgid))
#endif /* defined(__NR_setresgid) */

#if defined(__NR_getresgid)
DEFINE_SYSCALL_STUB(getresgid, (gid_t __user * rgid, gid_t __user *egid, gid_t __user *sgid))
#endif /* defined(__NR_getresgid) */

#if defined(__NR_prctl)
DEFINE_SYSCALL_STUB(prctl, (int option, unsigned long arg2, unsigned long arg3, unsigned long arg4,
                            unsigned long arg5))
#endif /* defined(__NR_prctl) */

#if defined(__NR_rt_sigreturn)
DEFINE_SYSCALL_STUB(rt_sigreturn, (void))
#endif /* defined(__NR_rt_sigreturn) */

#if defined(__NR_rt_sigaction)
DEFINE_SYSCALL_STUB(rt_sigaction, (int sig, const struct k_sigaction __user *act,
                                   struct k_sigaction __user *oact, size_t sigsetsize))
#endif /* defined(__NR_rt_sigaction) */

#if defined(__NR_rt_sigprocmask)
DEFINE_SYSCALL_STUB(rt_sigprocmask,
                    (int how, sigset_t __user *set, sigset_t __user *oset, size_t sigsetsize))
#endif /* defined(__NR_rt_sigprocmask) */

#if defined(__NR_rt_sigpending)
DEFINE_SYSCALL_STUB(rt_sigpending, (sigset_t __user * set, size_t sigsetsize))
#endif /* defined(__NR_rt_sigpending) */

#if defined(__NR_rt_sigtimedwait)
DEFINE_SYSCALL_STUB(rt_sigtimedwait, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                                      const struct timespec __user *uts, size_t sigsetsize))
#endif /* defined(__NR_rt_sigtimedwait) */

#if defined(__NR_rt_sigqueueinfo)
DEFINE_SYSCALL_STUB(rt_sigqueueinfo, (pid_t pid, int sig, siginfo_t __user *uinfo))
#endif /* defined(__NR_rt_sigqueueinfo) */

#if defined(__NR_rt_sigsuspend)
DEFINE_SYSCALL_STUB(rt_sigsuspend, (sigset_t __user * unewset, size_t sigsetsize))
#endif /* defined(__NR_rt_sigsuspend) */

#if defined(__NR_pread64)
DEFINE_SYSCALL_STUB(pread64, (int fd, char __user *buf, size_t count, loff_t pos))
#endif /* defined(__NR_pread64) */

#if defined(__NR_pwrite64)
DEFINE_SYSCALL_STUB(pwrite64, (int fd, const char __user *buf, size_t count, loff_t pos))
#endif /* defined(__NR_pwrite64) */

#if defined(__NR_chown)
DEFINE_SYSCALL_STUB(chown, (const char __user *filename, uid_t user, gid_t group))
#endif /* defined(__NR_chown) */

#if defined(__NR_getcwd)
DEFINE_SYSCALL_STUB(getcwd, (char __user *buf, unsigned long size))
#endif /* defined(__NR_getcwd) */

#if defined(__NR_capget)
DEFINE_SYSCALL_STUB(capget, (cap_user_header_t header, cap_user_data_t dataptr))
#endif /* defined(__NR_capget) */

#if defined(__NR_capset)
DEFINE_SYSCALL_STUB(capset, (cap_user_header_t header, const cap_user_data_t data))
#endif /* defined(__NR_capset) */

#if defined(__NR_sigaltstack)
DEFINE_SYSCALL_STUB(sigaltstack,
                    (const struct sigaltstack __user *uss, struct sigaltstack __user *uoss))
#endif /* defined(__NR_sigaltstack) */

#if defined(__NR_sendfile)
DEFINE_SYSCALL_STUB(sendfile, (int out_fd, int in_fd, off_t __user *offset, size_t count))
#endif /* defined(__NR_sendfile) */

#if defined(__NR_getpmsg)
DEFINE_SYSCALL_STUB(getpmsg, (void))
#endif /* defined(__NR_getpmsg) */
       /* linux not support */
#if defined(__NR_putpmsg)
DEFINE_SYSCALL_STUB(putpmsg, (void))
#endif /* defined(__NR_putpmsg) */
       /* linux not support */
#if defined(__NR_vfork)
DEFINE_SYSCALL_STUB(vfork, (void))
#endif /* defined(__NR_vfork) */

#if defined(__NR_ugetrlimit)
DEFINE_SYSCALL_STUB(ugetrlimit, (unsigned int resource, struct rlimit __user *rlim))
#endif /* defined(__NR_ugetrlimit) */
/* looklike getrlimit */
#if defined(__NR_mmap2)
DEFINE_SYSCALL_STUB(mmap2, (unsigned long addr, unsigned long len, int prot, int flags, int fd,
                            long pgoff))
#endif /* defined(__NR_mmap2) */

#if defined(__NR_truncate64)
DEFINE_SYSCALL_STUB(truncate64, (const char __user *path, loff_t length))
#endif /* defined(__NR_truncate64) */

#if defined(__NR_ftruncate64)
DEFINE_SYSCALL_STUB(ftruncate64, (int fd, loff_t length))
#endif /* defined(__NR_ftruncate64) */

#if defined(__NR_stat64)
DEFINE_SYSCALL_STUB(stat64, (const char __user *filename, struct stat64 __user *statbuf))
#endif /* defined(__NR_stat64) */

#if defined(__NR_lstat64)
DEFINE_SYSCALL_STUB(lstat64, (const char __user *filename, struct stat64 __user *statbuf))
#endif /* defined(__NR_lstat64) */

#if defined(__NR_fstat64)
DEFINE_SYSCALL_STUB(fstat64,
                    (int dfd, const char __user *filename, struct stat64 __user *statbuf, int flag))
#endif /* defined(__NR_fstat64) */

#if defined(__NR_lchown32)
DEFINE_SYSCALL_STUB(lchown32, (void))
#endif /* defined(__NR_lchown32) */
       /* linux not support */
#if defined(__NR_getuid32)
DEFINE_SYSCALL_STUB(getuid32, (void))
#endif /* defined(__NR_getuid32) */
       /* linux not support */
#if defined(__NR_getgid32)
DEFINE_SYSCALL_STUB(getgid32, (void))
#endif /* defined(__NR_getgid32) */
       /* linux not support */
#if defined(__NR_geteuid32)
DEFINE_SYSCALL_STUB(geteuid32, (void))
#endif /* defined(__NR_geteuid32) */
       /* linux not support */
#if defined(__NR_getegid32)
DEFINE_SYSCALL_STUB(getegid32, (void))
#endif /* defined(__NR_getegid32) */
       /* linux not support */
#if defined(__NR_setreuid32)
DEFINE_SYSCALL_STUB(setreuid32, (void))
#endif /* defined(__NR_setreuid32) */
       /* linux not support */
#if defined(__NR_setregid32)
DEFINE_SYSCALL_STUB(setregid32, (void))
#endif /* defined(__NR_setregid32) */
       /* linux not support */
#if defined(__NR_getgroups32)
DEFINE_SYSCALL_STUB(getgroups32, (int gidsetsize, gid_t __user *grouplist))
#endif /* defined(__NR_getgroups32) */
       /* linux not support */
#if defined(__NR_setgroups32)
DEFINE_SYSCALL_STUB(setgroups32, (int gidsetsize, gid_t __user *grouplist))
#endif /* defined(__NR_setgroups32) */
       /* linux not support */
#if defined(__NR_fchown32)
DEFINE_SYSCALL_STUB(fchown32, (int fd, uid_t uid, gid_t gid))
#endif /* defined(__NR_fchown32) */
       /* linux not support */
#if defined(__NR_setresuid32)
DEFINE_SYSCALL_STUB(setresuid32, (void))
#endif /* defined(__NR_setresuid32) */
       /* linux not support */
#if defined(__NR_getresuid32)
DEFINE_SYSCALL_STUB(getresuid32, (void))
#endif /* defined(__NR_getresuid32) */
       /* linux not support */
#if defined(__NR_setresgid32)
DEFINE_SYSCALL_STUB(setresgid32, (void))
#endif /* defined(__NR_setresgid32) */
       /* linux not support */
#if defined(__NR_getresgid32)
DEFINE_SYSCALL_STUB(getresgid32, (void))
#endif /* defined(__NR_getresgid32) */
       /* linux not support */
#if defined(__NR_chown32)
DEFINE_SYSCALL_STUB(chown32, (const char __user *filename, uid_t user, gid_t group))
#endif /* defined(__NR_chown32) */
       /* linux not support */
#if defined(__NR_setuid32)
DEFINE_SYSCALL_STUB(setuid32, (uid_t uid))
#endif /* defined(__NR_setuid32) */
       /* linux not support */
#if defined(__NR_setgid32)
DEFINE_SYSCALL_STUB(setgid32, (uid_t uid))
#endif /* defined(__NR_setgid32) */
       /* linux not support */
#if defined(__NR_setfsuid32)
DEFINE_SYSCALL_STUB(setfsuid32, (void))
#endif /* defined(__NR_setfsuid32) */
       /* linux not support */
#if defined(__NR_setfsgid32)
DEFINE_SYSCALL_STUB(setfsgid32, (void))
#endif /* defined(__NR_setfsgid32) */
       /* linux not support */
#if defined(__NR_pivot_root)
DEFINE_SYSCALL_STUB(pivot_root, (const char __user *new_root, const char __user *put_old))
#endif /* defined(__NR_pivot_root) */

#if defined(__NR_mincore)
DEFINE_SYSCALL_STUB(mincore, (unsigned long start, size_t len, unsigned char __user *vec))
#endif /* defined(__NR_mincore) */

#if defined(__NR_madvise)
DEFINE_SYSCALL_STUB(madvise, (unsigned long start, size_t len, int behavior))
#endif /* defined(__NR_madvise) */

#if defined(__NR_getdents64)
DEFINE_SYSCALL_STUB(getdents64, (int fd, struct dirent __user *dirent, unsigned int count))
#endif /* defined(__NR_getdents64) */

#if defined(__NR_fcntl64)
DEFINE_SYSCALL_STUB(fcntl64, (int fd, unsigned int cmd, unsigned long arg))
#endif /* defined(__NR_fcntl64) */

#if defined(__NR_gettid)
DEFINE_SYSCALL_STUB(gettid, (void))
#endif /* defined(__NR_gettid) */

#if defined(__NR_readahead)
DEFINE_SYSCALL_STUB(readahead, (int fd, loff_t offset, size_t count))
#endif /* defined(__NR_readahead) */

#if defined(__NR_setxattr)
DEFINE_SYSCALL_STUB(setxattr, (const char __user *path, const char __user *name,
                               const void __user *value, size_t size, int flags))
#endif /* defined(__NR_setxattr) */

#if defined(__NR_lsetxattr)
DEFINE_SYSCALL_STUB(lsetxattr, (const char __user *path, const char __user *name,
                                const void __user *value, size_t size, int flags))
#endif /* defined(__NR_lsetxattr) */

#if defined(__NR_fsetxattr)
DEFINE_SYSCALL_STUB(fsetxattr, (int fd, const char __user *name, const void __user *value,
                                size_t size, int flags))
#endif /* defined(__NR_fsetxattr) */

#if defined(__NR_getxattr)
DEFINE_SYSCALL_STUB(getxattr, (const char __user *path, const char __user *name, void __user *value,
                               size_t size))
#endif /* defined(__NR_getxattr) */

#if defined(__NR_lgetxattr)
DEFINE_SYSCALL_STUB(lgetxattr, (const char __user *path, const char __user *name,
                                void __user *value, size_t size))
#endif /* defined(__NR_lgetxattr) */

#if defined(__NR_fgetxattr)
DEFINE_SYSCALL_STUB(fgetxattr, (int fd, const char __user *name, void __user *value, size_t size))
#endif /* defined(__NR_fgetxattr) */

#if defined(__NR_listxattr)
DEFINE_SYSCALL_STUB(listxattr, (const char __user *path, char __user *list, size_t size))
#endif /* defined(__NR_listxattr) */

#if defined(__NR_llistxattr)
DEFINE_SYSCALL_STUB(llistxattr, (const char __user *path, char __user *list, size_t size))
#endif /* defined(__NR_llistxattr) */

#if defined(__NR_flistxattr)
DEFINE_SYSCALL_STUB(flistxattr, (int fd, char __user *list, size_t size))
#endif /* defined(__NR_flistxattr) */

#if defined(__NR_removexattr)
DEFINE_SYSCALL_STUB(removexattr, (const char __user *path, const char __user *name))
#endif /* defined(__NR_removexattr) */

#if defined(__NR_lremovexattr)
DEFINE_SYSCALL_STUB(lremovexattr, (const char __user *path, const char __user *name))
#endif /* defined(__NR_lremovexattr) */

#if defined(__NR_fremovexattr)
DEFINE_SYSCALL_STUB(fremovexattr, (int fd, const char __user *name))
#endif /* defined(__NR_fremovexattr) */

#if defined(__NR_tkill)
DEFINE_SYSCALL_STUB(tkill, (pid_t pid, int sig))
#endif /* defined(__NR_tkill) */

#if defined(__NR_sendfile64)
DEFINE_SYSCALL_STUB(sendfile64, (int out_fd, int in_fd, loff_t __user *offset, size_t count))
#endif /* defined(__NR_sendfile64) */

#if defined(__NR_futex)
DEFINE_SYSCALL_STUB(futex, (uint32_t __user * uaddr, int op, uint32_t val,
                            struct timespec __user *utime, uint32_t __user *uaddr2, uint32_t val3))
#endif /* defined(__NR_futex) */

#if defined(__NR_sched_setaffinity)
DEFINE_SYSCALL_STUB(sched_setaffinity,
                    (pid_t pid, unsigned int len, unsigned long __user *user_mask_ptr))
#endif /* defined(__NR_sched_setaffinity) */

#if defined(__NR_sched_getaffinity)
DEFINE_SYSCALL_STUB(sched_getaffinity,
                    (pid_t pid, unsigned int len, unsigned long __user *user_mask_ptr))
#endif /* defined(__NR_sched_getaffinity) */

#if defined(__NR_set_thread_area)
DEFINE_SYSCALL_STUB(set_thread_area, (void))
#endif /* defined(__NR_set_thread_area) */
       /* musl not use */
#if defined(__NR_get_thread_area)
DEFINE_SYSCALL_STUB(get_thread_area, (void))
#endif /* defined(__NR_get_thread_area) */
       /* musl not use */
#if defined(__NR_io_setup)
DEFINE_SYSCALL_STUB(io_setup, (void))
#endif /* defined(__NR_io_setup) */
       /* musl not use */
#if defined(__NR_io_destroy)
DEFINE_SYSCALL_STUB(io_destroy, (void))
#endif /* defined(__NR_io_destroy) */
       /* musl not use */
#if defined(__NR_io_getevents)
DEFINE_SYSCALL_STUB(io_getevents, (void))
#endif /* defined(__NR_io_getevents) */
       /* musl not use */
#if defined(__NR_io_submit)
DEFINE_SYSCALL_STUB(io_submit, (void))
#endif /* defined(__NR_io_submit) */
       /* musl not use */
#if defined(__NR_io_cancel)
DEFINE_SYSCALL_STUB(io_cancel, (void))
#endif /* defined(__NR_io_cancel) */
       /* musl not use */
#if defined(__NR_fadvise64)
DEFINE_SYSCALL_STUB(fadvise64, (int fd, loff_t offset, size_t len, int advice))
#endif /* defined(__NR_fadvise64) */

#if defined(__NR_exit_group)
DEFINE_SYSCALL_STUB(exit_group, (int error_code))
#endif /* defined(__NR_exit_group) */

#if defined(__NR_lookup_dcookie)
DEFINE_SYSCALL_STUB(lookup_dcookie, (uint64_t cookie64, char __user *buf, size_t len))
#endif /* defined(__NR_lookup_dcookie) */

#if defined(__NR_epoll_create)
DEFINE_SYSCALL_STUB(epoll_create, (int size))
#endif /* defined(__NR_epoll_create) */

#if defined(__NR_epoll_ctl)
DEFINE_SYSCALL_STUB(epoll_ctl, (int epfd, int op, int fd, struct epoll_event __user *event))
#endif /* defined(__NR_epoll_ctl) */

#if defined(__NR_epoll_wait)
DEFINE_SYSCALL_STUB(epoll_wait,
                    (int epfd, struct epoll_event __user *events, int maxevents, int timeout))
#endif /* defined(__NR_epoll_wait) */

#if defined(__NR_remap_file_pages)
DEFINE_SYSCALL_STUB(remap_file_pages, (unsigned long start, unsigned long size, unsigned long prot,
                                       unsigned long pgoff, unsigned long flags))
#endif /* defined(__NR_remap_file_pages) */

#if defined(__NR_set_tid_address)
DEFINE_SYSCALL_STUB(set_tid_address, (int __user *tidptr))
#endif /* defined(__NR_set_tid_address) */

#if defined(__NR_timer_create)
DEFINE_SYSCALL_STUB(timer_create, (clockid_t which_clock, struct sigevent __user *timer_event_spec,
                                   timer_t __user *created_timer_id))
#endif /* defined(__NR_timer_create) */

#if defined(__NR_timer_settime32)
DEFINE_SYSCALL_STUB(timer_settime32, (timer_t t, int flags, struct itimerspec32 __user *val,
                                      struct itimerspec32 __user *old))
#endif /* defined(__NR_timer_settime32) */
#if defined(__NR_timer_settime)
DEFINE_SYSCALL_STUB(timer_settime, (timer_t t, int flags, struct itimerspec __user *val,
                                    struct itimerspec __user *old))
#endif /* defined(__NR_timer_settime) */
       /* linux not support */
#if defined(__NR_timer_gettime32)
DEFINE_SYSCALL_STUB(timer_gettime32, (timer_t t, struct itimerspec32 __user *val))
#endif /* defined(__NR_timer_gettime32) */
       /* linux not support */
#if defined(__NR_timer_getoverrun)
DEFINE_SYSCALL_STUB(timer_getoverrun, (timer_t timer_id))
#endif /* defined(__NR_timer_getoverrun) */

#if defined(__NR_timer_delete)
DEFINE_SYSCALL_STUB(timer_delete, (timer_t timer_id))
#endif /* defined(__NR_timer_delete) */

#if defined(__NR_clock_settime)
DEFINE_SYSCALL_STUB(clock_settime, (clockid_t clk, const struct timespec __user *ts))
#endif /* defined(__NR_clock_settime32) */
#if defined(__NR_clock_gettime)
DEFINE_SYSCALL_STUB(clock_gettime, (clockid_t clk, struct timespec __user *ts))
#endif /* defined(__NR_clock_gettime) */

#if defined(__NR_clock_settime32)
DEFINE_SYSCALL_STUB(clock_settime32, (clockid_t clk, const struct timespec32 __user *ts));
#endif /* defined(__NR_clock_settime32) */
       /* linux not support */
#if defined(__NR_clock_gettime32)
DEFINE_SYSCALL_STUB(clock_gettime32, (clockid_t clk, struct timespec32 __user *ts))
#endif /* defined(__NR_clock_gettime32) */
       /* linux not support */
#if defined(__NR_clock_getres_time32)
DEFINE_SYSCALL_STUB(clock_getres_time32, (clockid_t clk, struct timespec32 __user *ts32))
#endif /* defined(__NR_clock_getres_time32) */
       /* linux not support */
#if defined(__NR_clock_nanosleep_time32)
DEFINE_SYSCALL_STUB(clock_nanosleep_time32,
                    (clockid_t clk, int flags, struct timespec32 __user *_rqtp,
                     struct timespec32 __user *_rmtp))
#endif /* defined(__NR_clock_nanosleep_time32) */
       /* linux not support */
#if defined(__NR_statfs64)
DEFINE_SYSCALL_STUB(statfs64, (const char __user *path, size_t sz, struct statfs64 __user *buf))
#endif /* defined(__NR_statfs64) */

#if defined(__NR_fstatfs64)
DEFINE_SYSCALL_STUB(fstatfs64, (int fd, size_t sz, struct statfs64 __user *buf))
#endif /* defined(__NR_fstatfs64) */

#if defined(__NR_tgkill)
DEFINE_SYSCALL_STUB(tgkill, (pid_t tgid, pid_t pid, int sig))
#endif /* defined(__NR_tgkill) */

#if defined(__NR_utimes)
DEFINE_SYSCALL_STUB(utimes, (char __user *filename, struct timeval __user *utimes))
#endif /* defined(__NR_utimes) */

#if defined(__NR_fadvise64_64)
DEFINE_SYSCALL_STUB(fadvise64_64, (int fd, loff_t offset, loff_t len, int advice))
#endif /* defined(__NR_fadvise64_64) */

#if defined(__NR_vserver)
DEFINE_SYSCALL_STUB(vserver, (void))
#endif /* defined(__NR_vserver) */
       /* linux not support */
#if defined(__NR_mbind)
DEFINE_SYSCALL_STUB(mbind,
                    (unsigned long start, unsigned long len, unsigned long mode,
                     const unsigned long __user *nmask, unsigned long maxnode, unsigned flags))
#endif /* defined(__NR_mbind) */

#if defined(__NR_get_mempolicy)
DEFINE_SYSCALL_STUB(get_mempolicy, (int __user *policy, unsigned long __user *nmask,
                                    unsigned long maxnode, unsigned long addr, unsigned long flags))
#endif /* defined(__NR_get_mempolicy) */

#if defined(__NR_set_mempolicy)
DEFINE_SYSCALL_STUB(set_mempolicy,
                    (int mode, const unsigned long __user *nmask, unsigned long maxnode))
#endif /* defined(__NR_set_mempolicy) */

#if defined(__NR_mq_open)
DEFINE_SYSCALL_STUB(mq_open,
                    (const char __user *name, int oflag, mode_t mode, struct mq_attr __user *attr))
#endif /* defined(__NR_mq_open) */

#if defined(__NR_mq_unlink)
DEFINE_SYSCALL_STUB(mq_unlink, (const char __user *name))
#endif /* defined(__NR_mq_unlink) */

#if defined(__NR_mq_timedsend)
DEFINE_SYSCALL_STUB(mq_timedsend,
                    (mqd_t mqdes, const char __user *msg_ptr, size_t msg_len, unsigned int msg_prio,
                     const struct timespec __user *abs_timeout))
#endif /* defined(__NR_mq_timedsend) */

#if defined(__NR_mq_timedreceive)
DEFINE_SYSCALL_STUB(mq_timedreceive,
                    (mqd_t mqdes, char __user *msg_ptr, size_t msg_len,
                     unsigned int __user *msg_prio, const struct timespec __user *abs_timeout))
#endif /* defined(__NR_mq_timedreceive) */

#if defined(__NR_mq_notify)
DEFINE_SYSCALL_STUB(mq_notify, (mqd_t mqdes, const struct sigevent __user *notification))
#endif /* defined(__NR_mq_notify) */

#if defined(__NR_mq_getsetattr)
DEFINE_SYSCALL_STUB(mq_getsetattr, (mqd_t mqdes, const struct mq_attr __user *mqstat,
                                    struct mq_attr __user *omqstat))
#endif /* defined(__NR_mq_getsetattr) */

#if defined(__NR_kexec_load)
DEFINE_SYSCALL_STUB(kexec_load, (void))
#endif /* defined(__NR_kexec_load) */
/* musl not use */
#if defined(__NR_waitid)
DEFINE_SYSCALL_STUB(waitid, (int which, pid_t pid, siginfo_t __user *infop, int options,
                             struct rusage __user *ru))
#endif /* defined(__NR_waitid) */

#if defined(__NR_add_key)
DEFINE_SYSCALL_STUB(add_key, (void))
#endif /* defined(__NR_add_key) */
/* musl not use */
#if defined(__NR_request_key)
DEFINE_SYSCALL_STUB(request_key, (void))
#endif /* defined(__NR_request_key) */
/* musl not use */
#if defined(__NR_keyctl)
DEFINE_SYSCALL_STUB(keyctl, (void))
#endif /* defined(__NR_keyctl) */
/* musl not use */
#if defined(__NR_ioprio_set)
DEFINE_SYSCALL_STUB(ioprio_set, (int which, int who, int ioprio))
#endif /* defined(__NR_ioprio_set) */

#if defined(__NR_ioprio_get)
DEFINE_SYSCALL_STUB(ioprio_get, (int which, int who))
#endif /* defined(__NR_ioprio_get) */

#if defined(__NR_inotify_init)
DEFINE_SYSCALL_STUB(inotify_init, (void))
#endif /* defined(__NR_inotify_init) */

#if defined(__NR_inotify_add_watch)
DEFINE_SYSCALL_STUB(inotify_add_watch, (int fd, const char __user *path, uint32_t mask))
#endif /* defined(__NR_inotify_add_watch) */

#if defined(__NR_inotify_rm_watch)
DEFINE_SYSCALL_STUB(inotify_rm_watch, (int fd, int32_t wd))
#endif /* defined(__NR_inotify_rm_watch) */

#if defined(__NR_migrate_pages)
DEFINE_SYSCALL_STUB(migrate_pages,
                    (pid_t pid, unsigned long maxnode, const unsigned long __user *from,
                     const unsigned long __user *to))
#endif /* defined(__NR_migrate_pages) */

#if defined(__NR_openat)
DEFINE_SYSCALL_STUB(openat, (int dfd, const char __user *filename, int flags, mode_t mode))
#endif /* defined(__NR_openat) */

#if defined(__NR_mkdirat)
DEFINE_SYSCALL_STUB(mkdirat, (int dfd, const char __user *pathname, mode_t mode))
#endif /* defined(__NR_mkdirat) */

#if defined(__NR_mknodat)
DEFINE_SYSCALL_STUB(mknodat, (int dfd, const char __user *filename, mode_t mode, unsigned dev))
#endif /* defined(__NR_mknodat) */

#if defined(__NR_fchownat)
DEFINE_SYSCALL_STUB(fchownat,
                    (int dfd, const char __user *filename, uid_t user, gid_t group, int flag))
#endif /* defined(__NR_fchownat) */

#if defined(__NR_futimesat)
DEFINE_SYSCALL_STUB(futimesat,
                    (int dfd, const char __user *filename, struct timeval __user *utimes))
#endif /* defined(__NR_futimesat) */

#if defined(__NR_fstatat64)
DEFINE_SYSCALL_STUB(fstatat64,
                    (int dfd, const char __user *filename, struct stat64 __user *statbuf, int flag))
#endif /* defined(__NR_fstatat64) */

#if defined(SYS_newfstatat)
DEFINE_SYSCALL_STUB(newfstatat,
                    (int dfd, const char __user *filename, struct stat __user *statbuf, int flag))
#endif /* defined(SYS_newfstatat) */

#if defined(__NR_unlinkat)
DEFINE_SYSCALL_STUB(unlinkat, (int dfd, const char __user *pathname, int flag))
#endif /* defined(__NR_unlinkat) */

#if defined(__NR_renameat)
DEFINE_SYSCALL_STUB(renameat, (int olddfd, const char __user *oldname, int newdfd,
                               const char __user *newname))
#endif /* defined(__NR_renameat) */

#if defined(__NR_linkat)
DEFINE_SYSCALL_STUB(linkat, (int olddfd, const char __user *oldname, int newdfd,
                             const char __user *newname, int flags))
#endif /* defined(__NR_linkat) */

#if defined(__NR_symlinkat)
DEFINE_SYSCALL_STUB(symlinkat, (const char __user *oldname, int newdfd, const char __user *newname))
#endif /* defined(__NR_symlinkat) */

#if defined(__NR_readlinkat)
DEFINE_SYSCALL_STUB(readlinkat, (int dfd, const char __user *path, char __user *buf, int bufsiz))
#endif /* defined(__NR_readlinkat) */

#if defined(__NR_fchmodat)
DEFINE_SYSCALL_STUB(fchmodat, (int dfd, const char __user *filename, mode_t mode))
#endif /* defined(__NR_fchmodat) */

#if defined(__NR_faccessat)
DEFINE_SYSCALL_STUB(faccessat, (int dfd, const char __user *filename, int mode))
#endif /* defined(__NR_faccessat) */

#if defined(__NR_pselect6)
DEFINE_SYSCALL_STUB(pselect6, (int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp,
                               struct timespec __user *tsp, void __user *sig))
#endif /* defined(__NR_pselect6) */

#if defined(__NR_ppoll)
DEFINE_SYSCALL_STUB(ppoll,
                    (struct pollfd __user * ufds, unsigned int nfds, struct timespec __user *tsp,
                     const sigset_t __user *sigmask, size_t sigsetsize))
#endif /* defined(__NR_ppoll) */

#if defined(__NR_unshare)
DEFINE_SYSCALL_STUB(unshare, (unsigned long unshare_flags))
#endif /* defined(__NR_unshare) */

#if defined(__NR_set_robust_list)
DEFINE_SYSCALL_STUB(set_robust_list, (void __user *head, size_t len))
#endif /* defined(__NR_set_robust_list) */

#if defined(__NR_get_robust_list)
DEFINE_SYSCALL_STUB(get_robust_list,
                    (int pid, void __user *__user *head_ptr, size_t __user *len_ptr))
#endif /* defined(__NR_get_robust_list) */

#if defined(__NR_splice)
DEFINE_SYSCALL_STUB(splice, (int fd_in, loff_t __user *off_in, int fd_out, loff_t __user *off_out,
                             size_t len, unsigned int flags))
#endif /* defined(__NR_splice) */

#if defined(__NR_sync_file_range)
DEFINE_SYSCALL_STUB(sync_file_range, (int fd, loff_t offset, loff_t nbytes, unsigned int flags))
#endif /* defined(__NR_sync_file_range) */

#if defined(__NR_tee)
DEFINE_SYSCALL_STUB(tee, (int fdin, int fdout, size_t len, unsigned int flags))
#endif /* defined(__NR_tee) */

#if defined(__NR_vmsplice)
DEFINE_SYSCALL_STUB(vmsplice, (int fd, const struct iovec __user *iov, unsigned long nr_segs,
                               unsigned int flags))
#endif /* defined(__NR_vmsplice) */

#if defined(__NR_move_pages)
DEFINE_SYSCALL_STUB(move_pages,
                    (pid_t pid, unsigned long nr_pages, const void __user *__user *pages,
                     const int __user *nodes, int __user *status, int flags))
#endif /* defined(__NR_move_pages) */

#if defined(__NR_getcpu)
DEFINE_SYSCALL_STUB(getcpu, (unsigned __user *cpu, unsigned __user *node, void __user *cache))
#endif /* defined(__NR_getcpu) */

#if defined(__NR_epoll_pwait)
DEFINE_SYSCALL_STUB(epoll_pwait, (int epfd, struct epoll_event __user *events, int maxevents,
                                  int timeout, const sigset_t __user *sigmask, size_t sigsetsize))
#endif /* defined(__NR_epoll_pwait) */

#if defined(__NR_utimensat)
DEFINE_SYSCALL_STUB(utimensat, (int dfd, const char __user *filename,
                                struct timespec __user *utimes, int flags))
#endif /* defined(__NR_utimensat) */

#if defined(__NR_signalfd)
DEFINE_SYSCALL_STUB(signalfd, (int ufd, sigset_t __user *user_mask, size_t sizemask))
#endif /* defined(__NR_signalfd) */

#if defined(__NR_timerfd_create)
DEFINE_SYSCALL_STUB(timerfd_create, (int clockid, int flags))
#endif /* defined(__NR_timerfd_create) */

#if defined(__NR_eventfd)
DEFINE_SYSCALL_STUB(eventfd, (unsigned int count))
#endif /* defined(__NR_eventfd) */

#if defined(__NR_fallocate)
DEFINE_SYSCALL_STUB(fallocate, (int fd, int mode, loff_t offset, loff_t len))
#endif /* defined(__NR_fallocate) */

#if defined(__NR_timerfd_settime32)
DEFINE_SYSCALL_STUB(timerfd_settime32, (void))
#endif /* defined(__NR_timerfd_settime32) */
       /* linux not support */
#if defined(__NR_timerfd_gettime32)
DEFINE_SYSCALL_STUB(timerfd_gettime32, (void))
#endif /* defined(__NR_timerfd_gettime32) */
       /* linux not support */
#if defined(__NR_signalfd4)
DEFINE_SYSCALL_STUB(signalfd4, (int ufd, sigset_t __user *user_mask, size_t sizemask, int flags))
#endif /* defined(__NR_signalfd4) */

#if defined(__NR_eventfd2)
DEFINE_SYSCALL_STUB(eventfd2, (unsigned int count, int flags))
#endif /* defined(__NR_eventfd2) */

#if defined(__NR_epoll_create1)
DEFINE_SYSCALL_STUB(epoll_create1, (int flags))
#endif /* defined(__NR_epoll_create1) */

#if defined(__NR_dup3)
DEFINE_SYSCALL_STUB(dup3, (unsigned int oldfd, unsigned int newfd, int flags))
#endif /* defined(__NR_dup3) */

#if defined(__NR_pipe2)
DEFINE_SYSCALL_STUB(pipe2, (int __user *fildes, int flags))
#endif /* defined(__NR_pipe2) */

#if defined(__NR_inotify_init1)
DEFINE_SYSCALL_STUB(inotify_init1, (int flags))
#endif /* defined(__NR_inotify_init1) */

#if defined(__NR_preadv)
DEFINE_SYSCALL_STUB(preadv, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen,
                             unsigned long pos_l, unsigned long pos_h))
#endif /* defined(__NR_preadv) */

#if defined(__NR_pwritev)
DEFINE_SYSCALL_STUB(pwritev, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen,
                              unsigned long pos_l, unsigned long pos_h))
#endif /* defined(__NR_pwritev) */

#if defined(__NR_rt_tgsigqueueinfo)
DEFINE_SYSCALL_STUB(rt_tgsigqueueinfo, (pid_t tgid, pid_t pid, int sig, siginfo_t __user *uinfo))
#endif /* defined(__NR_rt_tgsigqueueinfo) */

#if defined(__NR_perf_event_open)
DEFINE_SYSCALL_STUB(perf_event_open, (void))
#endif /* defined(__NR_perf_event_open) */
/* musl not use */
#if defined(__NR_recvmmsg)
DEFINE_SYSCALL_STUB(recvmmsg, (int fd, struct mmsghdr __user *msg, unsigned int vlen,
                               unsigned flags, struct timespec __user *timeout))
#endif /* defined(__NR_recvmmsg) */

#if defined(__NR_fanotify_init)
DEFINE_SYSCALL_STUB(fanotify_init, (unsigned int flags, unsigned int event_f_flags))
#endif /* defined(__NR_fanotify_init) */

#if defined(__NR_fanotify_mark)
DEFINE_SYSCALL_STUB(fanotify_mark, (int fanotify_fd, unsigned int flags, uint64_t mask, int fd,
                                    const char __user *pathname))
#endif /* defined(__NR_fanotify_mark) */

#if defined(__NR_prlimit64)
DEFINE_SYSCALL_STUB(prlimit64,
                    (pid_t pid, unsigned int resource, const struct rlimit64 __user *new_rlim,
                     struct rlimit64 __user *old_rlim))
#endif /* defined(__NR_prlimit64) */

#if defined(__NR_name_to_handle_at)
DEFINE_SYSCALL_STUB(name_to_handle_at,
                    (int dfd, const char __user *name, struct file_handle __user *handle,
                     int __user *mnt_id, int flag))
#endif /* defined(__NR_name_to_handle_at) */

#if defined(__NR_open_by_handle_at)
DEFINE_SYSCALL_STUB(open_by_handle_at,
                    (int mountdirfd, struct file_handle __user *handle, int flags))
#endif /* defined(__NR_open_by_handle_at) */

#if defined(__NR_clock_adjtime)
DEFINE_SYSCALL_STUB(clock_adjtime, (clockid_t which_clock, struct timex __user *tx))
#endif /* defined(__NR_clock_adjtime) */

#if defined(__NR_syncfs)
DEFINE_SYSCALL_STUB(syncfs, (int fd))
#endif /* defined(__NR_syncfs) */

#if defined(__NR_sendmmsg)
DEFINE_SYSCALL_STUB(sendmmsg,
                    (int fd, struct mmsghdr __user *msg, unsigned int vlen, unsigned flags))
#endif /* defined(__NR_sendmmsg) */

#if defined(__NR_setns)
DEFINE_SYSCALL_STUB(setns, (int fd, int nstype))
#endif /* defined(__NR_setns) */

#if defined(__NR_process_vm_readv)
DEFINE_SYSCALL_STUB(process_vm_readv,
                    (pid_t pid, const struct iovec __user *lvec, unsigned long liovcnt,
                     const struct iovec __user *rvec, unsigned long riovcnt, unsigned long flags))
#endif /* defined(__NR_process_vm_readv) */

#if defined(__NR_process_vm_writev)
DEFINE_SYSCALL_STUB(process_vm_writev,
                    (pid_t pid, const struct iovec __user *lvec, unsigned long liovcnt,
                     const struct iovec __user *rvec, unsigned long riovcnt, unsigned long flags))
#endif /* defined(__NR_process_vm_writev) */

#if defined(__NR_kcmp)
DEFINE_SYSCALL_STUB(kcmp,
                    (pid_t pid1, pid_t pid2, int type, unsigned long idx1, unsigned long idx2))
#endif /* defined(__NR_kcmp) */

#if defined(__NR_sched_setattr)
DEFINE_SYSCALL_STUB(sched_setattr, (void))
#endif /* defined(__NR_sched_setattr) */
/* musl not use */
#if defined(__NR_sched_getattr)
DEFINE_SYSCALL_STUB(sched_getattr, (void))
#endif /* defined(__NR_sched_getattr) */
/* musl not use */
#if defined(__NR_renameat2)
DEFINE_SYSCALL_STUB(renameat2, (int olddfd, const char __user *oldname, int newdfd,
                                const char __user *newname, unsigned int flags))
#endif /* defined(__NR_renameat2) */

#if defined(__NR_seccomp)
DEFINE_SYSCALL_STUB(seccomp, (unsigned int op, unsigned int flags, const char __user *uargs))
#endif /* defined(__NR_seccomp) */

#if defined(__NR_getrandom)
DEFINE_SYSCALL_STUB(getrandom, (char __user *buf, size_t count, unsigned int flags))
#endif /* defined(__NR_getrandom) */

#if defined(__NR_memfd_create)
DEFINE_SYSCALL_STUB(memfd_create, (const char __user *uname_ptr, unsigned int flags))
#endif /* defined(__NR_memfd_create) */

#if defined(__NR_bpf)
DEFINE_SYSCALL_STUB(bpf, (void))
#endif /* defined(__NR_bpf) */
       /* musl not use */
#if defined(__NR_execveat)
DEFINE_SYSCALL_STUB(execveat,
                    (int dfd, const char __user *filename, const char __user *const __user *argv,
                     const char __user *const __user *envp, int flags))
#endif /* defined(__NR_execveat) */

#if defined(__NR_socket)
DEFINE_SYSCALL_STUB(socket, (int family, int type, int protocol))
#endif /* defined(__NR_socket) */

#if defined(__NR_socketpair)
DEFINE_SYSCALL_STUB(socketpair, (int family, int type, int protocol, int __user *usockvec))
#endif /* defined(__NR_socketpair) */

#if defined(__NR_bind)
DEFINE_SYSCALL_STUB(bind, (int fd, struct sockaddr __user *umyaddr, int addrlen))
#endif /* defined(__NR_bind) */

#if defined(__NR_connect)
DEFINE_SYSCALL_STUB(connect, (int fd, struct sockaddr __user *uservaddr, int addrlen))
#endif /* defined(__NR_connect) */

#if defined(__NR_listen)
DEFINE_SYSCALL_STUB(listen, (int fd, int backlog))
#endif /* defined(__NR_listen) */

#if defined(__NR_accept)
DEFINE_SYSCALL_STUB(accept,
                    (int fd, struct sockaddr __user *upeer_sockaddr, int __user *upeer_addrlen))
#endif /* defined(__NR_accept) */

#if defined(__NR_accept4)
DEFINE_SYSCALL_STUB(accept4, (int fd, struct sockaddr __user *upeer_sockaddr,
                              int __user *upeer_addrlen, int flags))
#endif /* defined(__NR_accept4) */

#if defined(__NR_getsockopt)
DEFINE_SYSCALL_STUB(getsockopt,
                    (int fd, int level, int optname, char __user *optval, int __user *optlen))
#endif /* defined(__NR_getsockopt) */

#if defined(__NR_setsockopt)
DEFINE_SYSCALL_STUB(setsockopt, (int fd, int level, int optname, char __user *optval, int optlen))
#endif /* defined(__NR_setsockopt) */

#if defined(__NR_getsockname)
DEFINE_SYSCALL_STUB(getsockname,
                    (int fd, struct sockaddr __user *usockaddr, socklen_t __user *usockaddr_len))
#endif /* defined(__NR_getsockname) */

#if defined(__NR_getpeername)
DEFINE_SYSCALL_STUB(getpeername,
                    (int fd, struct sockaddr __user *usockaddr, socklen_t __user *usockaddr_len))
#endif /* defined(__NR_getpeername) */

#if defined(__NR_sendto)
DEFINE_SYSCALL_STUB(sendto, (int fd, void __user *buff, size_t len, unsigned flags,
                             struct sockaddr __user *addr, socklen_t addr_len))
#endif /* defined(__NR_sendto) */

#if defined(__NR_sendmsg)
DEFINE_SYSCALL_STUB(sendmsg, (int fd, struct msghdr __user *msg, unsigned flags))
#endif /* defined(__NR_sendmsg) */

#if defined(__NR_recvfrom)
DEFINE_SYSCALL_STUB(recvfrom, (int fd, void __user *ubuf, size_t size, unsigned flags,
                               struct sockaddr __user *addr, socklen_t __user *addr_len))
#endif /* defined(__NR_recvfrom) */

#if defined(__NR_recvmsg)
DEFINE_SYSCALL_STUB(recvmsg, (int fd, struct msghdr __user *msg, unsigned flags))
#endif /* defined(__NR_recvmsg) */

#if defined(__NR_shutdown)
DEFINE_SYSCALL_STUB(shutdown, (int fd, int how))
#endif /* defined(__NR_shutdown) */

#if defined(__NR_userfaultfd)
DEFINE_SYSCALL_STUB(userfaultfd, (int flags))
#endif /* defined(__NR_userfaultfd) */

#if defined(__NR_membarrier)
DEFINE_SYSCALL_STUB(membarrier, (int cmd, int flags))
#endif /* defined(__NR_membarrier) */

#if defined(__NR_mlock2)
DEFINE_SYSCALL_STUB(mlock2, (unsigned long start, size_t len, int flags))
#endif /* defined(__NR_mlock2) */

#if defined(__NR_copy_file_range)
DEFINE_SYSCALL_STUB(copy_file_range, (int fd_in, loff_t __user *off_in, int fd_out,
                                      loff_t __user *off_out, size_t len, unsigned int flags))
#endif /* defined(__NR_copy_file_range) */

#if defined(__NR_preadv2)
DEFINE_SYSCALL_STUB(preadv2, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen,
                              unsigned long pos_l, unsigned long pos_h, int flags))
#endif /* defined(__NR_preadv2) */
       /* musl not use */
#if defined(__NR_pwritev2)
DEFINE_SYSCALL_STUB(pwritev2, (unsigned long fd, const struct iovec __user *vec, unsigned long vlen,
                               unsigned long pos_l, unsigned long pos_h, int flags))
#endif /* defined(__NR_pwritev2) */
       /* musl not use */
#if defined(__NR_pkey_mprotect)
DEFINE_SYSCALL_STUB(pkey_mprotect, (unsigned long start, size_t len, unsigned long prot, int pkey))
#endif /* defined(__NR_pkey_mprotect) */

#if defined(__NR_pkey_alloc)
DEFINE_SYSCALL_STUB(pkey_alloc, (unsigned long flags, unsigned long init_val))
#endif /* defined(__NR_pkey_alloc) */

#if defined(__NR_pkey_free)
DEFINE_SYSCALL_STUB(pkey_free, (int pkey))
#endif /* defined(__NR_pkey_free) */

#if defined(__NR_statx)
DEFINE_SYSCALL_STUB(statx, (int dfd, const char __user *path, unsigned flags, unsigned mask,
                            void __user *buffer))
#endif /* defined(__NR_statx) */

#if defined(__NR_arch_prctl)
DEFINE_SYSCALL_STUB(arch_prctl, (int option, unsigned long arg2))
#endif /* defined(__NR_arch_prctl) */

#if defined(__NR_io_pgetevents)
DEFINE_SYSCALL_STUB(io_pgetevents, (void))
#endif /* defined(__NR_io_pgetevents) */
       /* linux not support */
#if defined(__NR_rseq)
DEFINE_SYSCALL_STUB(rseq, (void))
#endif /* defined(__NR_rseq) */

#if defined(__NR_semget)
DEFINE_SYSCALL_STUB(semget, (key_t key, int nsems, int semflg))
#endif /* defined(__NR_semget) */

#if defined(__NR_semctl)
DEFINE_SYSCALL_STUB(semctl, (int semid, int semnum, int cmd, unsigned long arg))
#endif /* defined(__NR_semctl) */

#if defined(__NR_shmget)
DEFINE_SYSCALL_STUB(shmget, (key_t key, size_t size, int flag))
#endif /* defined(__NR_shmget) */

#if defined(__NR_shmctl)
DEFINE_SYSCALL_STUB(shmctl, (int shmid, int cmd, struct shmid_ds __user *buf))
#endif /* defined(__NR_shmctl) */

#if defined(__NR_shmat)
DEFINE_SYSCALL_STUB(shmat, (int shmid, char __user *shmaddr, int shmflg))
#endif /* defined(__NR_shmat) */

#if defined(__NR_shmdt)
DEFINE_SYSCALL_STUB(shmdt, (char __user *shmaddr))
#endif /* defined(__NR_shmdt) */

#if defined(__NR_msgget)
DEFINE_SYSCALL_STUB(msgget, (key_t key, int msgflg))
#endif /* defined(__NR_msgget) */

#if defined(__NR_msgsnd)
DEFINE_SYSCALL_STUB(msgsnd, (int msqid, struct msgbuf __user *msgp, size_t msgsz, int msgflg))
#endif /* defined(__NR_msgsnd) */

#if defined(__NR_msgrcv)
DEFINE_SYSCALL_STUB(msgrcv,
                    (int msqid, struct msgbuf __user *msgp, size_t msgsz, long msgtyp, int msgflg))
#endif /* defined(__NR_msgrcv) */

#if defined(__NR_msgctl)
DEFINE_SYSCALL_STUB(msgctl, (int msqid, int cmd, struct msqid_ds __user *buf))
#endif /* defined(__NR_msgctl) */

#if defined(__NR_clock_gettime64)
DEFINE_SYSCALL_STUB(clock_gettime64, (clockid_t clk, struct timespec64 *ts))
#endif /* defined(__NR_clock_gettime64) */
       /* linux not support */
#if defined(__NR_clock_settime64)
DEFINE_SYSCALL_STUB(clock_settime64, (clockid_t clk, const struct timespec64 __user *ts))
#endif /* defined(__NR_clock_settime64) */
       /* linux not support */
#if defined(__NR_clock_adjtime64)
DEFINE_SYSCALL_STUB(clock_adjtime64, (void))
#endif /* defined(__NR_clock_adjtime64) */
       /* linux not support */

#if defined(__NR_clock_getres)
DEFINE_SYSCALL_STUB(clock_getres, (clockid_t clk, struct timespec __user *ts))
#endif /* defined(__NR_clock_getres) */

#if defined(__NR_clock_getres_time64)
DEFINE_SYSCALL_STUB(clock_getres_time64, (clockid_t clk, struct timespec64 __user *ts))
#endif /* defined(__NR_clock_getres_time64) */
       /* linux not support */
#if defined(__NR_clock_nanosleep_time64)
DEFINE_SYSCALL_STUB(clock_nanosleep_time64,
                    (clockid_t clk, int flags, struct timespec64 __user *_rqtp,
                     struct timespec64 __user *_rmtp))
#endif /* defined(__NR_clock_nanosleep_time64) */
       /* linux not support */
#if defined(__NR_timer_gettime64)
DEFINE_SYSCALL_STUB(timer_gettime64, (timer_t t, struct itimerspec64 __user *val))
#endif /* defined(__NR_timer_gettime64) */

#if defined(__NR_timer_gettime)
DEFINE_SYSCALL_STUB(timer_gettime, (timer_t t, struct itimerspec __user *val))
#endif /* defined(__NR_timer_gettime) */

#if defined(__NR_timer_settime64)
DEFINE_SYSCALL_STUB(timer_settime64, (timer_t t, int flags, struct itimerspec64 __user *val,
                                      struct itimerspec64 __user *old))
#endif /* defined(__NR_timer_settime64) */
       /* linux not support */
#if defined(__NR_timerfd_gettime64)
DEFINE_SYSCALL_STUB(timerfd_gettime64, (void))
#endif /* defined(__NR_timerfd_gettime64) */
       /* linux not support */
#if defined(__NR_timerfd_settime64)
DEFINE_SYSCALL_STUB(timerfd_settime64, (void))
#endif /* defined(__NR_timerfd_settime64) */
       /* linux not support */
#if defined(__NR_utimensat_time64)
DEFINE_SYSCALL_STUB(utimensat_time64, (void))
#endif /* defined(__NR_utimensat_time64) */
       /* linux not support */
#if defined(__NR_pselect6_time64)
DEFINE_SYSCALL_STUB(pselect6_time64, (void))
#endif /* defined(__NR_pselect6_time64) */
       /* linux not support */
#if defined(__NR_ppoll_time64)
DEFINE_SYSCALL_STUB(ppoll_time64, (void))
#endif /* defined(__NR_ppoll_time64) */
       /* linux not support */
#if defined(__NR_io_pgetevents_time64)
DEFINE_SYSCALL_STUB(io_pgetevents_time64, (void))
#endif /* defined(__NR_io_pgetevents_time64) */
       /* linux not support */
#if defined(__NR_recvmmsg_time64)
DEFINE_SYSCALL_STUB(recvmmsg_time64, (void))
#endif /* defined(__NR_recvmmsg_time64) */
       /* linux not support */
#if defined(__NR_mq_timedsend_time64)
DEFINE_SYSCALL_STUB(mq_timedsend_time64, (void))
#endif /* defined(__NR_mq_timedsend_time64) */
       /* linux not support */
#if defined(__NR_mq_timedreceive_time64)
DEFINE_SYSCALL_STUB(mq_timedreceive_time64, (void))
#endif /* defined(__NR_mq_timedreceive_time64) */
       /* linux not support */
#if defined(__NR_semtimedop_time64)
DEFINE_SYSCALL_STUB(semtimedop_time64, (void))
#endif /* defined(__NR_semtimedop_time64) */
       /* linux not support */
#if defined(__NR_rt_sigtimedwait_time64)
DEFINE_SYSCALL_STUB(rt_sigtimedwait_time64, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                                             const struct timespec __user *uts, size_t sigsetsize))
#endif /* defined(__NR_rt_sigtimedwait_time64) */
       /* linux not support */
#if defined(__NR_futex_time64)
DEFINE_SYSCALL_STUB(futex_time64, (void))
#endif /* defined(__NR_futex_time64) */
       /* linux not support */
#if defined(__NR_sched_rr_get_interval_time64)
DEFINE_SYSCALL_STUB(sched_rr_get_interval_time64, (void))
#endif /* defined(__NR_sched_rr_get_interval_time64) */
       /* linux not support */
#if defined(__NR_pidfd_send_signal)
DEFINE_SYSCALL_STUB(pidfd_send_signal, (void))
#endif /* defined(__NR_pidfd_send_signal) */
       /* linux not support */
#if defined(__NR_io_uring_setup)
DEFINE_SYSCALL_STUB(io_uring_setup, (void))
#endif /* defined(__NR_io_uring_setup) */
       /* linux not support */
#if defined(__NR_io_uring_enter)
DEFINE_SYSCALL_STUB(io_uring_enter, (void))
#endif /* defined(__NR_io_uring_enter) */
       /* linux not support */
#if defined(__NR_io_uring_register)
DEFINE_SYSCALL_STUB(io_uring_register, (void))
#endif /* defined(__NR_io_uring_register) */
       /* linux not support */
#if defined(__NR_open_tree)
DEFINE_SYSCALL_STUB(open_tree, (void))
#endif /* defined(__NR_open_tree) */
       /* linux not support */
#if defined(__NR_move_mount)
DEFINE_SYSCALL_STUB(move_mount, (void))
#endif /* defined(__NR_move_mount) */
       /* linux not support */
#if defined(__NR_fsopen)
DEFINE_SYSCALL_STUB(fsopen, (void))
#endif /* defined(__NR_fsopen) */
       /* linux not support */
#if defined(__NR_fsconfig)
DEFINE_SYSCALL_STUB(fsconfig, (void))
#endif /* defined(__NR_fsconfig) */
       /* linux not support */
#if defined(__NR_fsmount)
DEFINE_SYSCALL_STUB(fsmount, (void))
#endif /* defined(__NR_fsmount) */
       /* linux not support */
#if defined(__NR_fspick)
DEFINE_SYSCALL_STUB(fspick, (void))
#endif /* defined(__NR_fspick) */
       /* linux not support */
#if defined(__NR_pidfd_open)
DEFINE_SYSCALL_STUB(pidfd_open, (void))
#endif /* defined(__NR_pidfd_open) */
       /* linux not support */
#if defined(__NR_clone3)
DEFINE_SYSCALL_STUB(clone3, (void))
#endif /* defined(__NR_clone3) */
       /* linux not support */
#if defined(__NR_close_range)
DEFINE_SYSCALL_STUB(close_range, (void))
#endif /* defined(__NR_close_range) */
       /* linux not support */
#if defined(__NR_openat2)
DEFINE_SYSCALL_STUB(openat2, (void))
#endif /* defined(__NR_openat2) */
       /* linux not support */
#if defined(__NR_pidfd_getfd)
DEFINE_SYSCALL_STUB(pidfd_getfd, (void))
#endif /* defined(__NR_pidfd_getfd) */
       /* linux not support */
#if defined(__NR_faccessat2)
DEFINE_SYSCALL_STUB(faccessat2, (void))
#endif /* defined(__NR_faccessat2) */
       /* linux not support */
#if defined(__NR_process_madvise)
DEFINE_SYSCALL_STUB(process_madvise, (void))
#endif /* defined(__NR_process_madvise) */
       /* linux not support */
#if defined(__NR_epoll_pwait2)
DEFINE_SYSCALL_STUB(epoll_pwait2, (void))
#endif /* defined(__NR_epoll_pwait2) */
       /* linux not support */
#if defined(__NR_mount_setattr)
DEFINE_SYSCALL_STUB(mount_setattr, (void))
#endif /* defined(__NR_mount_setattr) */
       /* linux not support */
#if defined(__NR_landlock_create_ruleset)
DEFINE_SYSCALL_STUB(landlock_create_ruleset, (void))
#endif /* defined(__NR_landlock_create_ruleset) */
       /* linux not support */
#if defined(__NR_landlock_add_rule)
DEFINE_SYSCALL_STUB(landlock_add_rule, (void))
#endif /* defined(__NR_landlock_add_rule) */
       /* linux not support */
#if defined(__NR_landlock_restrict_self)
DEFINE_SYSCALL_STUB(landlock_restrict_self, (void))
#endif /* defined(__NR_landlock_restrict_self) */
       /* linux not support */
