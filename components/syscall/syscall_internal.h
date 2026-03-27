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

#ifndef INCLUDE_SYSCALL_INTERNAL_H_
#define INCLUDE_SYSCALL_INTERNAL_H_

#define _GNU_SOURCE 1

#include <context.h>
#include <dirent.h>
#include <fcntl.h>
#include <mqueue.h>
#include <poll.h>
#include <process_signal.h>
#include <sched.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/msg.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/timex.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include <system/compiler.h>
#include <time/ktime.h>
#include <time/kitimer.h>

#if defined(__arm__) || defined(__aarch64__)

#define CONFIG_CLONE_BACKWARDS 1

#endif

#undef KLOG_TAG
#define KLOG_TAG "Syscall"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

#define asmlinkage

typedef sighandler_t sighandler_t;

#define __alias_syscall(name, alias)                                           \
  extern typeof(syscall_entry_##name) syscall_entry_##alias                    \
      __attribute__((__alias__("syscall_entry_" #name)))

#ifndef __user
#define __user
#endif

typedef void *cap_user_header_t;
typedef void *cap_user_data_t;
struct k_sigaction;
struct sigaltstack;

#define DEFINE_SYSCALL(name, arglist)                                               \
    asmlinkage long syscall_entry_##name arglist

#define DECLARE_SYSCALL(name, argc, arglist)                                    \
    enum { __syscall__##name##_argc = argc};                                    \
    extern asmlinkage long syscall_entry_##name arglist

#define SYSCALL_FUNC(name) syscall_entry_##name

#define SYSCALL_ENTRY_ITEM(name) [__NR_##name] = (void *)syscall_entry_##name

#define SYSCALL_NAME_ITEM(name) [__NR_##name] = (void *)#name

#define SYSCALL_ARGC_ITEM(name) [__NR_##name] = __syscall__##name##_argc

#define DEFINE_SYSCALL_STUB(name, arglist)                                     \
  __weak asmlinkage long syscall_entry_##name arglist {                        \
    KLOG_E(#name " %d this syscall not support now", __LINE__);                \
    return -ENOSYS;                                                            \
  }

DECLARE_SYSCALL(restart_syscall, 0, (void));
DECLARE_SYSCALL(exit, 1, (int error_code));
DECLARE_SYSCALL(fork, 0, (void));
DECLARE_SYSCALL(read, 3, (int fd, char __user *buf, size_t count));
DECLARE_SYSCALL(write, 3, (int fd, const char __user *buf, size_t count));
DECLARE_SYSCALL(open, 3, (const char __user *filename, int flags, mode_t mode));
DECLARE_SYSCALL(close, 1, (int fd));
DECLARE_SYSCALL(waitpid, 3, (pid_t pid, int __user *stat_addr, int options));
DECLARE_SYSCALL(creat, 2, (const char __user *pathname, mode_t mode));
DECLARE_SYSCALL(link, 2, (const char __user *oldname, const char __user *newname));
DECLARE_SYSCALL(unlink, 1, (const char __user *pathname));
DECLARE_SYSCALL(execve, 3, (const char __user *filename,
                         const char __user *const __user *argv,
                         const char __user *const __user *envp));
DECLARE_SYSCALL(chdir, 1, (const char __user *filename));
DECLARE_SYSCALL(time, 1, (time_t __user * tloc));
DECLARE_SYSCALL(mknod, 3, (const char __user *filename, mode_t mode, unsigned dev));
DECLARE_SYSCALL(chmod, 2, (const char __user *filename, mode_t mode));
DECLARE_SYSCALL(lchown, 3, (const char __user *filename, uid_t user, gid_t group));
DECLARE_SYSCALL(break, 0, (void));
/* linux not support */
DECLARE_SYSCALL(oldstat, 0, (void));
/* linux not support */
DECLARE_SYSCALL(lseek, 3, (int fd, off_t offset, unsigned int whence));
DECLARE_SYSCALL(getpid, 0, (void));
DECLARE_SYSCALL(mount, 5, (char __user *dev_name, char __user *dir_name,
                 char __user *type, unsigned long flags, void __user *data));
DECLARE_SYSCALL(umount, 1, (char __user *name));
DECLARE_SYSCALL(setuid, 1, (uid_t uid));
DECLARE_SYSCALL(getuid, 0, (void));
DECLARE_SYSCALL(stime, 1, (time_t __user * tptr));
DECLARE_SYSCALL(alarm, 1, (unsigned int seconds));
DECLARE_SYSCALL(oldfstat, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pause, 0, (void));
DECLARE_SYSCALL(utime, 2, (char __user *filename, struct utimbuf __user *times));
DECLARE_SYSCALL(stty, 0, (void));
/* linux not support */
DECLARE_SYSCALL(gtty, 0, (void));
/* linux not support */
DECLARE_SYSCALL(access, 2, (const char __user *filename, int mode));
DECLARE_SYSCALL(nice, 1, (int increment));
DECLARE_SYSCALL(ftime, 0, (void));
/* linux not support */
DECLARE_SYSCALL(sync, 0, (void));
DECLARE_SYSCALL(kill, 2, (pid_t pid, int sig));
DECLARE_SYSCALL(rename, 2, (const char __user *oldname, const char __user *newname));
DECLARE_SYSCALL(mkdir, 2, (const char __user *pathname, mode_t mode));
DECLARE_SYSCALL(rmdir, 1, (const char __user *pathname));
DECLARE_SYSCALL(dup, 1, (unsigned int fildes));
DECLARE_SYSCALL(pipe, 1, (int __user *fildes));
DECLARE_SYSCALL(times, 1, (struct tms __user * tbuf));
DECLARE_SYSCALL(prof, 0, (void));
/* linux not support */
DECLARE_SYSCALL(brk, 1, (unsigned long brk));
DECLARE_SYSCALL(setgid, 1, (gid_t gid));
DECLARE_SYSCALL(getgid, 0, (void));
DECLARE_SYSCALL(signal, 2, (int sig, sighandler_t handler));
DECLARE_SYSCALL(geteuid, 0, (void));
DECLARE_SYSCALL(getegid, 0, (void));
DECLARE_SYSCALL(acct, 1, (const char __user *name));
DECLARE_SYSCALL(umount2, 2, (char __user *name, int flags));
/* linux not support */
DECLARE_SYSCALL(lock, 0, (void));
/* linux not support */
DECLARE_SYSCALL(ioctl, 3, (int fd, unsigned int cmd, unsigned long arg));
DECLARE_SYSCALL(fcntl, 3, (int fd, unsigned int cmd, unsigned long arg));
DECLARE_SYSCALL(mpx, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setpgid, 2, (pid_t pid, pid_t pgid));
DECLARE_SYSCALL(ulimit, 0, (void));
/* linux not support */
DECLARE_SYSCALL(oldolduname, 0, (void));
/* linux not support */
DECLARE_SYSCALL(umask, 1, (int mask));
DECLARE_SYSCALL(chroot, 1, (const char __user *filename));
DECLARE_SYSCALL(ustat, 0, (void));
/* musl not use */
DECLARE_SYSCALL(dup2, 2, (unsigned int oldfd, unsigned int newfd));
DECLARE_SYSCALL(getppid, 0, (void));
DECLARE_SYSCALL(getpgrp, 0, (void));
DECLARE_SYSCALL(setsid, 0, (void));
DECLARE_SYSCALL(sigaction, 3, (int sig, const struct k_sigaction __user *act,
                            struct k_sigaction __user *oact));
/* linux drop */
DECLARE_SYSCALL(sgetmask, 0, (void));
DECLARE_SYSCALL(ssetmask, 1, (int newmask));
DECLARE_SYSCALL(setreuid, 2, (uid_t ruid, uid_t euid));
DECLARE_SYSCALL(setregid, 2, (gid_t rgid, gid_t egid));
DECLARE_SYSCALL(kernel_signal_suspend, 1, (sigset_t mask));
/* linux drop */
DECLARE_SYSCALL(sigpending, 1, (sigset_t __user * set));
DECLARE_SYSCALL(sethostname, 2, (char __user *name, int len));
DECLARE_SYSCALL(setrlimit, 2, (unsigned int resource, struct rlimit __user *rlim));
DECLARE_SYSCALL(getrlimit, 2, (unsigned int resource, struct rlimit __user *rlim));
DECLARE_SYSCALL(getrusage, 2, (int who, struct rusage __user *ru));
DECLARE_SYSCALL(gettimeofday_time32, 2, (struct timeval __user * tp, struct timezone __user *tzp));
DECLARE_SYSCALL(gettimeofday, 2, (struct timeval __user * tp, struct timezone __user *tzp));
/* linux not support */
DECLARE_SYSCALL(settimeofday_time32, 2, (const struct __user timeval *,
                                      const struct __user timezone *));
DECLARE_SYSCALL(settimeofday, 2, (const struct __user timeval *,
                               const struct __user timezone *));
DECLARE_SYSCALL(getgroups, 2, (int gidsetsize, gid_t __user *grouplist));
DECLARE_SYSCALL(setgroups, 2, (int gidsetsize, gid_t __user *grouplist));
DECLARE_SYSCALL(select, 5, (int n, fd_set __user *inp, fd_set __user *outp,
                         fd_set __user *exp, struct timeval __user *tvp));
DECLARE_SYSCALL(symlink, 2, (const char __user *, const char __user *));
DECLARE_SYSCALL(oldlstat, 0, (void));
/* linux not support */
DECLARE_SYSCALL(readlink, 3, (const char __user *path, char __user *buf, int bufsiz));
DECLARE_SYSCALL(uselib, 1, (const char __user *library));
DECLARE_SYSCALL(swapon, 2, (const char __user *specialfile, int swap_flags));
DECLARE_SYSCALL(reboot, 4, (int magic1, int magic2, unsigned int cmd, void __user *arg));
DECLARE_SYSCALL(readdir, 0, (void));
/* linux not support */
DECLARE_SYSCALL(mmap, 6, (unsigned long addr, unsigned long len, unsigned long prot,
                 unsigned long flags, int fd, off_t off));
DECLARE_SYSCALL(munmap, 2, (unsigned long addr, size_t len));
DECLARE_SYSCALL(truncate, 2, (const char __user *path, long length));
DECLARE_SYSCALL(ftruncate, 2, (int fd, loff_t length));
DECLARE_SYSCALL(fchmod, 2, (int fd, mode_t mode));
DECLARE_SYSCALL(fchown, 3, (int fd, uid_t user, gid_t group));
DECLARE_SYSCALL(getpriority, 2, (int which, int who));
DECLARE_SYSCALL(setpriority, 3, (int which, int who, int niceval));
DECLARE_SYSCALL(profil, 0, (void));
/* linux not support */
DECLARE_SYSCALL(statfs, 2, (const char __user *path, struct statfs __user *buf));
DECLARE_SYSCALL(fstatfs, 2, (int fd, struct statfs __user *buf));
DECLARE_SYSCALL(ioperm, 3, (unsigned long from, unsigned long num, int on));
DECLARE_SYSCALL(socketcall, 2, (int call, unsigned long __user *args));
DECLARE_SYSCALL(syslog, 3, (int type, char __user *buf, int len));
DECLARE_SYSCALL(setitimer, 3, (int which, struct itimerval __user *value,
                            struct itimerval __user *ovalue));
DECLARE_SYSCALL(getitimer, 2, (int which, struct itimerval __user *value));
DECLARE_SYSCALL(stat, 2, (const char __user *filename, struct stat __user *statbuf));
DECLARE_SYSCALL(lstat, 2, (const char __user *filename, struct stat __user *statbuf));
DECLARE_SYSCALL(fstat, 2, (int fd, struct stat __user *statbuf));
DECLARE_SYSCALL(olduname, 0, (void));
/* musl not use */
DECLARE_SYSCALL(iopl, 1, (unsigned int level));
DECLARE_SYSCALL(vhangup, 0, (void));
DECLARE_SYSCALL(idle, 0, (void));
DECLARE_SYSCALL(vm86old, 0, (void));
/* musl not use */
DECLARE_SYSCALL(wait4, 4, (pid_t pid, int __user *stat_addr, int options,
                        struct rusage __user *ru));
DECLARE_SYSCALL(swapoff, 1, (const char __user *specialfile));
DECLARE_SYSCALL(sysinfo, 1, (struct sysinfo __user * info));
DECLARE_SYSCALL(ipc, 6, (unsigned int call, int first, unsigned long second,
                      unsigned long third, void __user *ptr, long fifth));
DECLARE_SYSCALL(fsync, 1, (int fd));
DECLARE_SYSCALL(sigreturn, 0, (void));
#ifdef CONFIG_CLONE_BACKWARDS
DECLARE_SYSCALL(clone, 5, (unsigned long clone_flags, unsigned long newsp,
                        int __user *set_child_tid, unsigned long tls,
                        int __user *clear_child_tid));
#elif defined(CONFIG_CLONE_BACKWARDS2)
DECLARE_SYSCALL(clone, 5, (unsigned long newsp, unsigned long clone_flags,
                        int __user *set_child_tid, int __user *clear_child_tid,
                        unsigned long tls));

#elif defined(CONFIG_CLONE_BACKWARDS3)
DECLARE_SYSCALL(clone, 6, (unsigned long clone_flags, unsigned long newsp,
                        int stack_size, int __user *set_child_tid,
                        int __user *clear_child_tid, unsigned long tls));

#else
DECLARE_SYSCALL(clone, 5, (unsigned long clone_flags, unsigned long newsp,
                        int __user *set_child_tid, int __user *clear_child_tid,
                        unsigned long tls));

#endif
DECLARE_SYSCALL(setdomainname, 2, (char __user *name, int len));
DECLARE_SYSCALL(uname, 1, (struct utsname __user * name));
DECLARE_SYSCALL(modify_ldt, 3, (int func, void __user *ptr, unsigned long bytecount));
DECLARE_SYSCALL(adjtimex, 1, (struct timex __user * txc_p));
DECLARE_SYSCALL(mprotect, 3, (unsigned long start, size_t len, unsigned long prot));
DECLARE_SYSCALL(sigprocmask, 3, (int how, sigset_t __user *set, sigset_t __user *oset));
DECLARE_SYSCALL(quotactl, 4, (int cmd, const char __user *special, int id,
                           void __user *addr));
DECLARE_SYSCALL(getpgid, 1, (pid_t pid));
DECLARE_SYSCALL(fchdir, 1, (int fd));
DECLARE_SYSCALL(bdflush, 2, (int func, long data));
DECLARE_SYSCALL(sysfs, 3, (int option, unsigned long arg1, unsigned long arg2));
DECLARE_SYSCALL(personality, 1, (unsigned int personality));
DECLARE_SYSCALL(afs_syscall, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setfsuid, 1, (uid_t uid));
DECLARE_SYSCALL(setfsgid, 1, (gid_t gid));
DECLARE_SYSCALL(_llseek, 5, (int fd, unsigned long offset_high, unsigned long offset_low,
                 loff_t __user *result, unsigned int whence));
DECLARE_SYSCALL(getdents, 3, (int fd, struct dirent __user *dirent, unsigned int count));
DECLARE_SYSCALL(_newselect, 5, (int n, fd_set __user *inp, fd_set __user *outp,
                             fd_set __user *exp, struct timeval __user *tvp));
/* linux not support */
DECLARE_SYSCALL(flock, 2, (int fd, unsigned int cmd));
DECLARE_SYSCALL(msync, 3, (unsigned long start, size_t len, int flags));
DECLARE_SYSCALL(readv, 3, (unsigned long fd, const struct iovec __user *vec,
                        unsigned long vlen));
DECLARE_SYSCALL(writev, 3, (unsigned long fd, const struct iovec __user *vec,
                         unsigned long vlen));
DECLARE_SYSCALL(getsid, 1, (pid_t pid));
DECLARE_SYSCALL(fdatasync, 1, (int fd));
DECLARE_SYSCALL(_sysctl, 0, (void));
/* musl not use */
DECLARE_SYSCALL(mlock, 2, (unsigned long start, size_t len));
DECLARE_SYSCALL(munlock, 2, (unsigned long start, size_t len));
DECLARE_SYSCALL(mlockall, 1, (int flags));
DECLARE_SYSCALL(munlockall, 0, (void));
DECLARE_SYSCALL(sched_setparam, 2, (pid_t pid, struct sched_param __user *param));
DECLARE_SYSCALL(sched_getparam, 2, (pid_t pid, struct sched_param __user *param));
DECLARE_SYSCALL(sched_setscheduler, 3, (pid_t pid, int policy, struct sched_param __user *param));
DECLARE_SYSCALL(sched_getscheduler, 1, (pid_t pid));
DECLARE_SYSCALL(sched_yield, 0, (void));
DECLARE_SYSCALL(sched_get_priority_max, 1, (int policy));
DECLARE_SYSCALL(sched_get_priority_min, 1, (int policy));
DECLARE_SYSCALL(sched_rr_get_interval, 2, (pid_t pid, struct timespec __user *interval));
DECLARE_SYSCALL(nanosleep, 2, (long __user *rqtp, long __user *rmtp));
DECLARE_SYSCALL(mremap, 5, (unsigned long addr, unsigned long old_len,
                         unsigned long new_len, unsigned long flags,
                         unsigned long new_addr));
DECLARE_SYSCALL(setresuid, 3, (uid_t ruid, uid_t euid, uid_t suid));
DECLARE_SYSCALL(getresuid, 3, (uid_t __user * ruid, uid_t __user *euid, uid_t __user *suid));
DECLARE_SYSCALL(vm86, 2, (unsigned long cmd, unsigned long arg));
DECLARE_SYSCALL(query_module, 0, (void));
/* linux not support */
DECLARE_SYSCALL(poll, 3, (struct pollfd __user * ufds, unsigned int nfds, int timeout));
DECLARE_SYSCALL(nfsservctl, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setresgid, 3, (gid_t rgid, gid_t egid, gid_t sgid));
DECLARE_SYSCALL(getresgid, 3, (gid_t __user * rgid, gid_t __user *egid, gid_t __user *sgid));
DECLARE_SYSCALL(prctl, 5, (int option, unsigned long arg2, unsigned long arg3,
                        unsigned long arg4, unsigned long arg5));
DECLARE_SYSCALL(rt_sigreturn, 0, (void));
DECLARE_SYSCALL(rt_sigaction, 4, (int sig, const struct k_sigaction __user *act,
                 struct k_sigaction __user *oact, size_t sigsetsize));
DECLARE_SYSCALL(rt_sigprocmask, 4, (int how, sigset_t __user *set,
                                 sigset_t __user *oset, size_t sigsetsize));
DECLARE_SYSCALL(rt_sigpending, 2, (sigset_t __user * set, size_t sigsetsize));
DECLARE_SYSCALL(rt_sigtimedwait, 4, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                 const struct timespec __user *uts, size_t sigsetsize));
DECLARE_SYSCALL(rt_sigqueueinfo, 3, (pid_t pid, int sig, siginfo_t __user *uinfo));
DECLARE_SYSCALL(rt_sigsuspend, 2, (sigset_t __user * unewset, size_t sigsetsize));
DECLARE_SYSCALL(pread64, 4, (int fd, char __user *buf, size_t count, loff_t pos));
DECLARE_SYSCALL(pwrite64, 4, (int fd, const char __user *buf, size_t count, loff_t pos));
DECLARE_SYSCALL(chown, 3, (const char __user *filename, uid_t user, gid_t group));
DECLARE_SYSCALL(getcwd, 2, (char __user *buf, unsigned long size));
DECLARE_SYSCALL(capget, 2, (cap_user_header_t header, cap_user_data_t dataptr));
DECLARE_SYSCALL(capset, 2, (cap_user_header_t header, const cap_user_data_t data));
DECLARE_SYSCALL(sigaltstack, 2, (const struct sigaltstack __user *uss,
                              struct sigaltstack __user *uoss));
DECLARE_SYSCALL(sendfile, 4, (int out_fd, int in_fd, off_t __user *offset, size_t count));
DECLARE_SYSCALL(getpmsg, 0, (void));
/* linux not support */
DECLARE_SYSCALL(putpmsg, 0, (void));
/* linux not support */
DECLARE_SYSCALL(vfork, 0, (void));
DECLARE_SYSCALL(ugetrlimit, 2, (unsigned int resource, struct rlimit __user *rlim));
/* looklike getrlimit */
DECLARE_SYSCALL(mmap2, 6, (unsigned long addr, unsigned long len, int prot,
                        int flags, int fd, long pgoff));
DECLARE_SYSCALL(truncate64, 2, (const char __user *path, loff_t length));
DECLARE_SYSCALL(ftruncate64, 2, (int fd, loff_t length));
DECLARE_SYSCALL(stat64, 2, (const char __user *filename, struct stat64 __user *statbuf));
DECLARE_SYSCALL(lstat64, 2, (const char __user *filename, struct stat64 __user *statbuf));
DECLARE_SYSCALL(fstat64, 4, (int dfd, const char __user *filename,
                          struct stat64 __user *statbuf, int flag));
DECLARE_SYSCALL(lchown32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getgid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(geteuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getegid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setreuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setregid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getgroups32, 2, (int gidsetsize, gid_t __user *grouplist));
/* linux not support */
DECLARE_SYSCALL(setgroups32, 2, (int gidsetsize, gid_t __user *grouplist));
/* linux not support */
DECLARE_SYSCALL(fchown32, 3, (int fd, uid_t uid, gid_t gid));
/* linux not support */
DECLARE_SYSCALL(setresuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getresuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setresgid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(getresgid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(chown32, 3, (const char __user *filename, uid_t user, gid_t group));
/* linux not support */
DECLARE_SYSCALL(setuid32, 1, (uid_t uid));
/* linux not support */
DECLARE_SYSCALL(setgid32, 1, (uid_t uid));
/* linux not support */
DECLARE_SYSCALL(setfsuid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(setfsgid32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pivot_root, 2, (const char __user *new_root, const char __user *put_old));
DECLARE_SYSCALL(mincore, 3, (unsigned long start, size_t len, unsigned char __user *vec));
DECLARE_SYSCALL(madvise, 3, (unsigned long start, size_t len, int behavior));
DECLARE_SYSCALL(getdents64, 3, (int fd, struct dirent __user *dirent, unsigned int count));
DECLARE_SYSCALL(fcntl64, 3, (int fd, unsigned int cmd, unsigned long arg));
DECLARE_SYSCALL(gettid, 0, (void));
DECLARE_SYSCALL(readahead, 3, (int fd, loff_t offset, size_t count));
DECLARE_SYSCALL(setxattr, 5, (const char __user *path, const char __user *name,
                           const void __user *value, size_t size, int flags));
DECLARE_SYSCALL(lsetxattr, 5, (const char __user *path, const char __user *name,
                            const void __user *value, size_t size, int flags));
DECLARE_SYSCALL(fsetxattr, 5, (int fd, const char __user *name,
                            const void __user *value, size_t size, int flags));
DECLARE_SYSCALL(getxattr, 4, (const char __user *path, const char __user *name,
                           void __user *value, size_t size));
DECLARE_SYSCALL(lgetxattr, 4, (const char __user *path, const char __user *name,
                            void __user *value, size_t size));
DECLARE_SYSCALL(fgetxattr, 4, (int fd, const char __user *name, void __user *value,
                            size_t size));
DECLARE_SYSCALL(listxattr, 3, (const char __user *path, char __user *list, size_t size));
DECLARE_SYSCALL(llistxattr, 3, (const char __user *path, char __user *list, size_t size));
DECLARE_SYSCALL(flistxattr, 3, (int fd, char __user *list, size_t size));
DECLARE_SYSCALL(removexattr, 2, (const char __user *path, const char __user *name));
DECLARE_SYSCALL(lremovexattr, 2, (const char __user *path, const char __user *name));
DECLARE_SYSCALL(fremovexattr, 2, (int fd, const char __user *name));
DECLARE_SYSCALL(tkill, 2, (pid_t pid, int sig));
DECLARE_SYSCALL(sendfile64, 4, (int out_fd, int in_fd, loff_t __user *offset, size_t count));
DECLARE_SYSCALL(futex, 6, (uint32_t __user * uaddr, int op, uint32_t val,
                        struct timespec __user *utime, uint32_t __user *uaddr2,
                        uint32_t val3));
DECLARE_SYSCALL(sched_setaffinity, 3, (pid_t pid, unsigned int len,
                                    unsigned long __user *user_mask_ptr));
DECLARE_SYSCALL(sched_getaffinity, 3, (pid_t pid, unsigned int len,
                                    unsigned long __user *user_mask_ptr));
DECLARE_SYSCALL(set_thread_area, 0, (void));
/* musl not use */
DECLARE_SYSCALL(get_thread_area, 0, (void));
/* musl not use */
DECLARE_SYSCALL(io_setup, 0, (void));
/* musl not use */
DECLARE_SYSCALL(io_destroy, 0, (void));
/* musl not use */
DECLARE_SYSCALL(io_getevents, 0, (void));
/* musl not use */
DECLARE_SYSCALL(io_submit, 0, (void));
/* musl not use */
DECLARE_SYSCALL(io_cancel, 0, (void));
/* musl not use */
DECLARE_SYSCALL(fadvise64, 4, (int fd, loff_t offset, size_t len, int advice));
DECLARE_SYSCALL(exit_group, 1, (int error_code));
DECLARE_SYSCALL(lookup_dcookie, 3, (uint64_t cookie64, char __user *buf, size_t len));
DECLARE_SYSCALL(epoll_create, 1, (int size));
DECLARE_SYSCALL(epoll_ctl, 4, (int epfd, int op, int fd, struct epoll_event __user *event));
DECLARE_SYSCALL(epoll_wait, 4, (int epfd, struct epoll_event __user *events,
                             int maxevents, int timeout));
DECLARE_SYSCALL(remap_file_pages, 5, (unsigned long start, unsigned long size, unsigned long prot,
                 unsigned long pgoff, unsigned long flags));
DECLARE_SYSCALL(set_tid_address, 1, (int __user *tidptr));
DECLARE_SYSCALL(timer_create, 3, (clockid_t which_clock,
                               struct sigevent __user *timer_event_spec,
                               timer_t __user *created_timer_id));
DECLARE_SYSCALL(timer_settime32, 4, (timer_t t, int flags, struct itimerspec32 __user *val,
                 struct itimerspec32 __user *old));
DECLARE_SYSCALL(timer_settime, 4, (timer_t t, int flags, struct itimerspec __user *val,
                 struct itimerspec __user *old));
/* linux not support */
DECLARE_SYSCALL(timer_gettime32, 2, (timer_t t, struct itimerspec32 __user *val));
/* linux not support */
DECLARE_SYSCALL(timer_getoverrun, 1, (timer_t timer_id));
DECLARE_SYSCALL(timer_delete, 1, (timer_t timer_id));
DECLARE_SYSCALL(clock_settime, 2, (clockid_t clk, const struct timespec __user *ts));
DECLARE_SYSCALL(clock_gettime, 2, (clockid_t clk, struct timespec __user *ts));
DECLARE_SYSCALL(clock_settime32, 2, (clockid_t clk, const struct timespec32 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_gettime32, 2, (clockid_t clk, struct timespec32 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_getres, 2, (clockid_t clk, struct timespec __user *ts));
DECLARE_SYSCALL(clock_getres_time32, 2, (clockid_t clk, struct timespec32 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_nanosleep_time32, 4, (clockid_t clk, int flags, struct timespec32 __user *_rqtp,
                 struct timespec32 __user *_rmtp));
/* linux not support */
DECLARE_SYSCALL(statfs64, 3, (const char __user *path, size_t sz,
                           struct statfs64 __user *buf));
DECLARE_SYSCALL(fstatfs64, 3, (int fd, size_t sz, struct statfs64 __user *buf));
DECLARE_SYSCALL(tgkill, 3, (pid_t tgid, pid_t pid, int sig));
DECLARE_SYSCALL(utimes, 2, (char __user *filename, struct timeval __user *utimes));
DECLARE_SYSCALL(fadvise64_64, 4, (int fd, loff_t offset, loff_t len, int advice));
DECLARE_SYSCALL(vserver, 0, (void));
/* linux not support */
DECLARE_SYSCALL(mbind, 6, (unsigned long start, unsigned long len,
                        unsigned long mode, const unsigned long __user *nmask,
                        unsigned long maxnode, unsigned flags));
DECLARE_SYSCALL(get_mempolicy, 5, (int __user *policy, unsigned long __user *nmask,
                                unsigned long maxnode, unsigned long addr,
                                unsigned long flags));
DECLARE_SYSCALL(set_mempolicy, 3, (int mode, const unsigned long __user *nmask,
                                unsigned long maxnode));
DECLARE_SYSCALL(mq_open, 4, (const char __user *name, int oflag, mode_t mode,
                          struct mq_attr __user *attr));
DECLARE_SYSCALL(mq_unlink, 1, (const char __user *name));
DECLARE_SYSCALL(mq_timedsend, 5, (mqd_t mqdes, const char __user *msg_ptr,
                               size_t msg_len, unsigned int msg_prio,
                               const struct timespec __user *abs_timeout));
DECLARE_SYSCALL(mq_timedreceive, 5, (mqd_t mqdes, char __user *msg_ptr,
                                  size_t msg_len, unsigned int __user *msg_prio,
                                  const struct timespec __user *abs_timeout));
DECLARE_SYSCALL(mq_notify, 2, (mqd_t mqdes, const struct sigevent __user *notification));
DECLARE_SYSCALL(mq_getsetattr, 3, (mqd_t mqdes, const struct mq_attr __user *mqstat,
                 struct mq_attr __user *omqstat));
DECLARE_SYSCALL(kexec_load, 0, (void));
/* musl not use */
DECLARE_SYSCALL(waitid, 5, (int which, pid_t pid, siginfo_t __user *infop,
                         int options, struct rusage __user *ru));
DECLARE_SYSCALL(add_key, 0, (void));
/* musl not use */
DECLARE_SYSCALL(request_key, 0, (void));
/* musl not use */
DECLARE_SYSCALL(keyctl, 0, (void));
/* musl not use */
DECLARE_SYSCALL(ioprio_set, 3, (int which, int who, int ioprio));
DECLARE_SYSCALL(ioprio_get, 2, (int which, int who));
DECLARE_SYSCALL(inotify_init, 0, (void));
DECLARE_SYSCALL(inotify_add_watch, 3, (int fd, const char __user *path, uint32_t mask));
DECLARE_SYSCALL(inotify_rm_watch, 2, (int fd, int32_t wd));
DECLARE_SYSCALL(migrate_pages, 4, (pid_t pid, unsigned long maxnode,
                                const unsigned long __user *from,
                                const unsigned long __user *to));
DECLARE_SYSCALL(openat, 4, (int dfd, const char __user *filename, int flags, mode_t mode));
DECLARE_SYSCALL(mkdirat, 3, (int dfd, const char __user *pathname, mode_t mode));
DECLARE_SYSCALL(mknodat, 4, (int dfd, const char __user *filename, mode_t mode,
                          unsigned dev));
DECLARE_SYSCALL(fchownat, 5, (int dfd, const char __user *filename, uid_t user,
                           gid_t group, int flag));
DECLARE_SYSCALL(futimesat, 3, (int dfd, const char __user *filename,
                            struct timeval __user *utimes));
DECLARE_SYSCALL(fstatat64, 4, (int dfd, const char __user *filename,
                            struct stat64 __user *statbuf, int flag));
DECLARE_SYSCALL(newfstatat, 4, (int dfd, const char __user *filename,
                             struct stat __user *statbuf, int flag));
DECLARE_SYSCALL(unlinkat, 3, (int dfd, const char __user *pathname, int flag));
DECLARE_SYSCALL(renameat, 4, (int olddfd, const char __user *oldname, int newdfd,
                           const char __user *newname));
DECLARE_SYSCALL(linkat, 5, (int olddfd, const char __user *oldname, int newdfd,
                         const char __user *newname, int flags));
DECLARE_SYSCALL(symlinkat, 3, (const char __user *oldname, int newdfd,
                            const char __user *newname));
DECLARE_SYSCALL(readlinkat, 4, (int dfd, const char __user *path, char __user *buf,
                             int bufsiz));
DECLARE_SYSCALL(fchmodat, 3, (int dfd, const char __user *filename, mode_t mode));
DECLARE_SYSCALL(faccessat, 3, (int dfd, const char __user *filename, int mode));
DECLARE_SYSCALL(pselect6, 6, (int n, fd_set __user *inp, fd_set __user *outp,
                           fd_set __user *exp, struct timespec __user *tsp,
                           void __user *sig));
DECLARE_SYSCALL(ppoll, 5, (struct pollfd __user * ufds, unsigned int nfds,
                        struct timespec __user *tsp,
                        const sigset_t __user *sigmask, size_t sigsetsize));
DECLARE_SYSCALL(unshare, 1, (unsigned long unshare_flags));
DECLARE_SYSCALL(set_robust_list, 2, (void __user *head, size_t len));
DECLARE_SYSCALL(get_robust_list, 3, (int pid, void __user *__user *head_ptr,
                                  size_t __user *len_ptr));
DECLARE_SYSCALL(splice, 6, (int fd_in, loff_t __user *off_in, int fd_out,
                 loff_t __user *off_out, size_t len, unsigned int flags));
DECLARE_SYSCALL(sync_file_range, 4, (int fd, loff_t offset, loff_t nbytes, unsigned int flags));
DECLARE_SYSCALL(tee, 4, (int fdin, int fdout, size_t len, unsigned int flags));
DECLARE_SYSCALL(vmsplice, 4, (int fd, const struct iovec __user *iov,
                           unsigned long nr_segs, unsigned int flags));
DECLARE_SYSCALL(move_pages, 6, (pid_t pid, unsigned long nr_pages,
                 const void __user *__user *pages, const int __user *nodes,
                 int __user *status, int flags));
DECLARE_SYSCALL(getcpu, 3, (unsigned __user *cpu, unsigned __user *node,
                         void __user *cache));
DECLARE_SYSCALL(epoll_pwait, 6, (int epfd, struct epoll_event __user *events, int maxevents,
                 int timeout, const sigset_t __user *sigmask,
                 size_t sigsetsize));
DECLARE_SYSCALL(utimensat, 4, (int dfd, const char __user *filename,
                            struct timespec __user *utimes, int flags));
DECLARE_SYSCALL(signalfd, 3, (int ufd, sigset_t __user *user_mask, size_t sizemask));
DECLARE_SYSCALL(timerfd_create, 2, (int clockid, int flags));
DECLARE_SYSCALL(eventfd, 1, (unsigned int count));
DECLARE_SYSCALL(fallocate, 4, (int fd, int mode, loff_t offset, loff_t len));
DECLARE_SYSCALL(timerfd_settime32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(timerfd_gettime32, 0, (void));
/* linux not support */
DECLARE_SYSCALL(signalfd4, 4, (int ufd, sigset_t __user *user_mask,
                            size_t sizemask, int flags));
DECLARE_SYSCALL(eventfd2, 2, (unsigned int count, int flags));
DECLARE_SYSCALL(epoll_create1, 1, (int flags));
DECLARE_SYSCALL(dup3, 3, (unsigned int oldfd, unsigned int newfd, int flags));
DECLARE_SYSCALL(pipe2, 2, (int __user *fildes, int flags));
DECLARE_SYSCALL(inotify_init1, 1, (int flags));
DECLARE_SYSCALL(preadv, 5, (unsigned long fd, const struct iovec __user *vec,
                 unsigned long vlen, unsigned long pos_l, unsigned long pos_h));
DECLARE_SYSCALL(pwritev, 5, (unsigned long fd, const struct iovec __user *vec,
                 unsigned long vlen, unsigned long pos_l, unsigned long pos_h));
DECLARE_SYSCALL(rt_tgsigqueueinfo, 4, (pid_t tgid, pid_t pid, int sig, siginfo_t __user *uinfo));
DECLARE_SYSCALL(perf_event_open, 0, (void));
/* musl not use */
DECLARE_SYSCALL(recvmmsg, 5, (int fd, struct mmsghdr __user *msg, unsigned int vlen,
                 unsigned flags, struct timespec __user *timeout));
DECLARE_SYSCALL(fanotify_init, 2, (unsigned int flags, unsigned int event_f_flags));
DECLARE_SYSCALL(fanotify_mark, 5, (int fanotify_fd, unsigned int flags, uint64_t mask, int fd,
                 const char __user *pathname));
DECLARE_SYSCALL(prlimit64, 4, (pid_t pid, unsigned int resource,
                            const struct rlimit64 __user *new_rlim,
                            struct rlimit64 __user *old_rlim));
DECLARE_SYSCALL(name_to_handle_at, 5, (int dfd, const char __user *name,
                                    struct file_handle __user *handle,
                                    int __user *mnt_id, int flag));
DECLARE_SYSCALL(open_by_handle_at, 3, (int mountdirfd, struct file_handle __user *handle, int flags));
DECLARE_SYSCALL(clock_adjtime, 2, (clockid_t which_clock, struct timex __user *tx));
DECLARE_SYSCALL(syncfs, 1, (int fd));
DECLARE_SYSCALL(sendmmsg, 4, (int fd, struct mmsghdr __user *msg,
                           unsigned int vlen, unsigned flags));
DECLARE_SYSCALL(setns, 2, (int fd, int nstype));
DECLARE_SYSCALL(process_vm_readv, 6, (pid_t pid, const struct iovec __user *lvec,
                 unsigned long liovcnt, const struct iovec __user *rvec,
                 unsigned long riovcnt, unsigned long flags));
DECLARE_SYSCALL(process_vm_writev, 6, (pid_t pid, const struct iovec __user *lvec,
                 unsigned long liovcnt, const struct iovec __user *rvec,
                 unsigned long riovcnt, unsigned long flags));
DECLARE_SYSCALL(kcmp, 5, (pid_t pid1, pid_t pid2, int type, unsigned long idx1,
                       unsigned long idx2));
DECLARE_SYSCALL(sched_setattr, 0, (void));
/* musl not use */
DECLARE_SYSCALL(sched_getattr, 0, (void));
/* musl not use */
DECLARE_SYSCALL(renameat2, 5, (int olddfd, const char __user *oldname, int newdfd,
                            const char __user *newname, unsigned int flags));
DECLARE_SYSCALL(seccomp, 3, (unsigned int op, unsigned int flags,
                          const char __user *uargs));
DECLARE_SYSCALL(getrandom, 3, (char __user *buf, size_t count, unsigned int flags));
DECLARE_SYSCALL(memfd_create, 2, (const char __user *uname_ptr, unsigned int flags));
DECLARE_SYSCALL(bpf, 0, (void));
/* musl not use */
DECLARE_SYSCALL(execveat, 5, (int dfd, const char __user *filename,
                           const char __user *const __user *argv,
                           const char __user *const __user *envp, int flags));
DECLARE_SYSCALL(socket, 3, (int family, int type, int protocol));
DECLARE_SYSCALL(socketpair, 4, (int family, int type, int protocol, int __user *usockvec));
DECLARE_SYSCALL(bind, 3, (int fd, struct sockaddr __user *umyaddr, int addrlen));
DECLARE_SYSCALL(connect, 3, (int fd, struct sockaddr __user *uservaddr, int addrlen));
DECLARE_SYSCALL(listen, 2, (int fd, int backlog));
DECLARE_SYSCALL(accept, 3, (int fd, struct sockaddr __user *upeer_sockaddr,
                         int __user *upeer_addrlen));
DECLARE_SYSCALL(accept4, 4, (int fd, struct sockaddr __user *upeer_sockaddr,
                          int __user *upeer_addrlen, int flags));
DECLARE_SYSCALL(getsockopt, 5, (int fd, int level, int optname,
                             char __user *optval, int __user *optlen));
DECLARE_SYSCALL(setsockopt, 5, (int fd, int level, int optname,
                             char __user *optval, int optlen));
DECLARE_SYSCALL(getsockname, 3, (int fd, struct sockaddr __user *usockaddr,
                              socklen_t __user *usockaddr_len));
DECLARE_SYSCALL(getpeername, 3, (int fd, struct sockaddr __user *usockaddr,
                              socklen_t __user *usockaddr_len));
DECLARE_SYSCALL(sendto, 6, (int fd, void __user *buff, size_t len, unsigned flags,
                         struct sockaddr __user *addr, socklen_t addr_len));
DECLARE_SYSCALL(sendmsg, 3, (int fd, struct msghdr __user *msg, unsigned flags));
DECLARE_SYSCALL(recvfrom, 6, (int fd, void __user *ubuf, size_t size, unsigned flags,
                 struct sockaddr __user *addr, socklen_t __user *addr_len));
DECLARE_SYSCALL(recvmsg, 3, (int fd, struct msghdr __user *msg, unsigned flags));
DECLARE_SYSCALL(shutdown, 2, (int fd, int how));
DECLARE_SYSCALL(userfaultfd, 1, (int flags));
DECLARE_SYSCALL(membarrier, 2, (int cmd, int flags));
DECLARE_SYSCALL(mlock2, 3, (unsigned long start, size_t len, int flags));
DECLARE_SYSCALL(copy_file_range, 6, (int fd_in, loff_t __user *off_in, int fd_out,
                 loff_t __user *off_out, size_t len, unsigned int flags));
DECLARE_SYSCALL(preadv2, 6, (unsigned long fd, const struct iovec __user *vec,
                          unsigned long vlen, unsigned long pos_l,
                          unsigned long pos_h, int flags));
/* musl not use */
DECLARE_SYSCALL(pwritev2, 6, (unsigned long fd, const struct iovec __user *vec,
                           unsigned long vlen, unsigned long pos_l,
                           unsigned long pos_h, int flags));
/* musl not use */
DECLARE_SYSCALL(pkey_mprotect, 4, (unsigned long start, size_t len,
                                unsigned long prot, int pkey));
DECLARE_SYSCALL(pkey_alloc, 2, (unsigned long flags, unsigned long init_val));
DECLARE_SYSCALL(pkey_free, 1, (int pkey));
DECLARE_SYSCALL(statx, 5, (int dfd, const char __user *path, unsigned flags,
                        unsigned mask, void __user *buffer));
DECLARE_SYSCALL(arch_prctl, 2, (int option, unsigned long arg2));
DECLARE_SYSCALL(io_pgetevents, 0, (void));
/* linux not support */
DECLARE_SYSCALL(rseq, 0, (void));
DECLARE_SYSCALL(semget, 3, (key_t key, int nsems, int semflg));
DECLARE_SYSCALL(semctl, 4, (int semid, int semnum, int cmd, unsigned long arg));
DECLARE_SYSCALL(shmget, 3, (key_t key, size_t size, int flag));
DECLARE_SYSCALL(shmctl, 3, (int shmid, int cmd, struct shmid_ds __user *buf));
DECLARE_SYSCALL(shmat, 3, (int shmid, char __user *shmaddr, int shmflg));
DECLARE_SYSCALL(shmdt, 1, (char __user *shmaddr));
DECLARE_SYSCALL(msgget, 2, (key_t key, int msgflg));
DECLARE_SYSCALL(msgsnd, 4, (int msqid, struct msgbuf __user *msgp, size_t msgsz,
                         int msgflg));
DECLARE_SYSCALL(msgrcv, 5, (int msqid, struct msgbuf __user *msgp, size_t msgsz,
                         long msgtyp, int msgflg));
DECLARE_SYSCALL(msgctl, 3, (int msqid, int cmd, struct msqid_ds __user *buf));
DECLARE_SYSCALL(clock_gettime64, 2, (clockid_t clk, struct timespec64 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_settime64, 2, (clockid_t clk, const struct timespec64 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_adjtime64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(clock_getres_time64, 2, (clockid_t clk, struct timespec64 __user *ts));
/* linux not support */
DECLARE_SYSCALL(clock_nanosleep, 4, (clockid_t clk, int flags, struct timespec *rqtp,
                 struct timespec *rmtp));
DECLARE_SYSCALL(clock_nanosleep_time64, 4, (clockid_t clk, int flags, struct timespec64 __user *_rqtp,
                 struct timespec64 __user *_rmtp));
/* linux not support */
DECLARE_SYSCALL(timer_gettime64, 2, (timer_t t, struct itimerspec64 __user *val));
DECLARE_SYSCALL(timer_gettime, 2, (timer_t t, struct itimerspec __user *val));
/* linux not support */
DECLARE_SYSCALL(timer_settime64, 4, (timer_t t, int flags, struct itimerspec64 __user *val,
                 struct itimerspec64 __user *old));
/* linux not support */
DECLARE_SYSCALL(timerfd_gettime64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(timerfd_settime64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(utimensat_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pselect6_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(ppoll_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(io_pgetevents_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(recvmmsg_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(mq_timedsend_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(mq_timedreceive_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(semtimedop_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(rt_sigtimedwait_time64, 4, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                 const struct timespec __user *uts, size_t sigsetsize));
/* linux not support */
DECLARE_SYSCALL(futex_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(sched_rr_get_interval_time64, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pidfd_send_signal, 0, (void));
/* linux not support */
DECLARE_SYSCALL(io_uring_setup, 0, (void));
/* linux not support */
DECLARE_SYSCALL(io_uring_enter, 0, (void));
/* linux not support */
DECLARE_SYSCALL(io_uring_register, 0, (void));
/* linux not support */
DECLARE_SYSCALL(open_tree, 0, (void));
/* linux not support */
DECLARE_SYSCALL(move_mount, 0, (void));
/* linux not support */
DECLARE_SYSCALL(fsopen, 0, (void));
/* linux not support */
DECLARE_SYSCALL(fsconfig, 0, (void));
/* linux not support */
DECLARE_SYSCALL(fsmount, 0, (void));
/* linux not support */
DECLARE_SYSCALL(fspick, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pidfd_open, 0, (void));
/* linux not support */
DECLARE_SYSCALL(clone3, 0, (void));
/* linux not support */
DECLARE_SYSCALL(close_range, 0, (void));
/* linux not support */
DECLARE_SYSCALL(openat2, 0, (void));
/* linux not support */
DECLARE_SYSCALL(pidfd_getfd, 0, (void));
/* linux not support */
DECLARE_SYSCALL(faccessat2, 0, (void));
/* linux not support */
DECLARE_SYSCALL(process_madvise, 0, (void));
/* linux not support */
DECLARE_SYSCALL(epoll_pwait2, 0, (void));
/* linux not support */
DECLARE_SYSCALL(mount_setattr, 0, (void));
/* linux not support */
DECLARE_SYSCALL(landlock_create_ruleset, 0, (void));
/* linux not support */
DECLARE_SYSCALL(landlock_add_rule, 0, (void));
/* linux not support */
DECLARE_SYSCALL(landlock_restrict_self, 0, (void));
/* linux not support */


#endif /* INCLUDE_SYSCALL_INTERNAL_H_ */
