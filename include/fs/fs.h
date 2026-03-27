/****************************************************************************
 * include/fs/fs.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __INCLUDE_NUTTX_FS_FS_H
#define __INCLUDE_NUTTX_FS_FS_H

#include <dirent.h>
#include <fs/kpoll.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <process_signal.h>

#ifndef __user
#define __user
#endif

#ifndef IPTR
#define IPTR
#endif

#ifndef printflike
#define printflike(a, b) __attribute__((__format__(__printf__, a, b)))
#endif

#ifndef OPEN_MAX
#define OPEN_MAX _POSIX_OPEN_MAX
#endif

#define RWF_HIPRI (0x00000001)

#define RWF_DSYNC (0x00000002)

#define RWF_SYNC (0x00000004)

#define RWF_NOWAIT (0x00000008)

#define RWF_APPEND (0x00000010)

#define RWF_NOAPPEND (0x00000020)

#define RWF_ATOMIC (0x00000040)

#define RWF_SUPPORTED                                                                              \
    (RWF_HIPRI | RWF_DSYNC | RWF_SYNC | RWF_NOWAIT | RWF_APPEND | RWF_NOAPPEND | RWF_ATOMIC)

#define __FS_FLAG_EOF (1 << 0)
#define __FS_FLAG_ERROR (1 << 1)
#define __FS_FLAG_LBF (1 << 2)
#define __FS_FLAG_UBF (1 << 3)

#define FSNODEFLAG_TYPE_MASK 0x0000000f
#define FSNODEFLAG_TYPE_PSEUDODIR 0x00000000
#define FSNODEFLAG_TYPE_DRIVER 0x00000001
#define FSNODEFLAG_TYPE_BLOCK 0x00000002
#define FSNODEFLAG_TYPE_MOUNTPT 0x00000003
#define FSNODEFLAG_TYPE_NAMEDSEM 0x00000004
#define FSNODEFLAG_TYPE_MQUEUE 0x00000005
#define FSNODEFLAG_TYPE_SHM 0x00000006
#define FSNODEFLAG_TYPE_MTD 0x00000007
#define FSNODEFLAG_TYPE_SOFTLINK 0x00000008
#define FSNODEFLAG_TYPE_SOCKET 0x00000009
#define FSNODEFLAG_TYPE_PIPE 0x0000000a
#define FSNODEFLAG_DELETED 0x00000010

#define INODE_IS_TYPE(i, t) (((i)->i_flags & FSNODEFLAG_TYPE_MASK) == (t))

#define INODE_IS_PSEUDODIR(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_PSEUDODIR)
#define INODE_IS_DRIVER(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_DRIVER)
#define INODE_IS_BLOCK(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_BLOCK)
#define INODE_IS_MOUNTPT(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_MOUNTPT)
#define INODE_IS_NAMEDSEM(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_NAMEDSEM)
#define INODE_IS_MQUEUE(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_MQUEUE)
#define INODE_IS_SHM(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_SHM)
#define INODE_IS_MTD(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_MTD)
#define INODE_IS_SOFTLINK(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_SOFTLINK)
#define INODE_IS_SOCKET(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_SOCKET)
#define INODE_IS_PIPE(i) INODE_IS_TYPE(i, FSNODEFLAG_TYPE_PIPE)

#define INODE_GET_TYPE(i) ((i)->i_flags & FSNODEFLAG_TYPE_MASK)
#define INODE_SET_TYPE(i, t)                                                                       \
    do                                                                                             \
    {                                                                                              \
        (i)->i_flags = ((i)->i_flags & ~FSNODEFLAG_TYPE_MASK) | (t);                               \
    } while (0)

#define INODE_SET_DRIVER(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_DRIVER)
#define INODE_SET_BLOCK(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_BLOCK)
#define INODE_SET_MOUNTPT(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_MOUNTPT)
#define INODE_SET_NAMEDSEM(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_NAMEDSEM)
#define INODE_SET_MQUEUE(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_MQUEUE)
#define INODE_SET_SHM(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_SHM)
#define INODE_SET_MTD(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_MTD)
#define INODE_SET_SOFTLINK(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_SOFTLINK)
#define INODE_SET_SOCKET(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_SOCKET)
#define INODE_SET_PIPE(i) INODE_SET_TYPE(i, FSNODEFLAG_TYPE_PIPE)

#define CH_STAT_MODE (1 << 0)
#define CH_STAT_UID (1 << 1)
#define CH_STAT_GID (1 << 2)
#define CH_STAT_ATIME (1 << 3)
#define CH_STAT_MTIME (1 << 4)

struct file;
struct inode;
struct stat;
struct statfs;
struct pollfd;
struct mtd_dev_s;
struct device;

struct fs_dirent_s
{
    struct inode *fd_root;

    char *fd_path;
};
struct mm_region;
struct file_operations
{
    int (*open)(struct file *filep);

    int (*close)(struct file *filep);
    ssize_t (*read)(struct file *filep, char __user *buffer, size_t buflen);
    ssize_t (*write)(struct file *filep, const char __user *buffer, size_t buflen);
    off_t (*seek)(struct file *filep, off_t offset, int whence);
    int (*ioctl)(struct file *filep, unsigned int cmd, unsigned long arg);
    int (*mmap)(struct file *filep, struct mm_region *map);

    int (*poll)(struct file *filep, struct kpollfd *fds, bool setup);
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
    int (*unlink)(struct inode *inode);
#endif
};

#ifndef CONFIG_DISABLE_MOUNTPOINT
struct geometry
{
    bool geo_available;
    bool geo_mediachanged;
    bool geo_writeenabled;
    blkcnt_t geo_nsectors;
    blksize_t geo_sectorsize;
};

struct partition_info_s
{
    size_t numsectors;
    size_t sectorsize;
    off_t startsector;

    char parent[NAME_MAX + 1];
};

struct inode;
struct block_operations
{
    int (*open)(struct inode *inode);
    int (*close)(struct inode *inode);
    ssize_t (*read)(struct inode *inode, unsigned char *buffer, blkcnt_t start_sector,
                    unsigned int nsectors);
    ssize_t (*write)(struct inode *inode, const unsigned char *buffer, blkcnt_t start_sector,
                     unsigned int nsectors);
    int (*geometry)(struct inode *inode, struct geometry *geometry);
    int (*ioctl)(struct inode *inode, int cmd, unsigned long arg);
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
    int (*unlink)(struct inode *inode);
#endif
};

struct mountpt_operations
{
    int (*open)(struct file *filep, const char *relpath, int oflags, mode_t mode);

    int (*close)(struct file *filep);
    ssize_t (*read)(struct file *filep, char *buffer, size_t buflen);
    ssize_t (*write)(struct file *filep, const char *buffer, size_t buflen);
    off_t (*seek)(struct file *filep, off_t offset, int whence);
    int (*ioctl)(struct file *filep, int cmd, unsigned long arg);

    int (*mmap)(struct file *filep, struct mm_region *map);
    int (*poll)(struct file *filep, struct pollfd *fds, bool setup);

    int (*sync)(struct file *filep);
    int (*dup)(const struct file *oldp, struct file *newp);
    int (*fstat)(const struct file *filep, struct stat *buf);
    int (*fchstat)(const struct file *filep, const struct stat *buf, int flags);
    int (*truncate)(struct file *filep, off_t length);

    int (*opendir)(struct inode *mountpt, const char *relpath, struct fs_dirent_s **dir);
    int (*closedir)(struct inode *mountpt, struct fs_dirent_s *dir);
    int (*readdir)(struct inode *mountpt, struct fs_dirent_s *dir, struct dirent *entry);
    int (*rewinddir)(struct inode *mountpt, struct fs_dirent_s *dir);

    int (*bind)(struct inode *blkdriver, const void *data, void **handle);
    int (*unbind)(void *handle, struct inode **blkdriver, unsigned int flags);
    int (*statfs)(struct inode *mountpt, struct statfs *buf);

    int (*unlink)(struct inode *mountpt, const char *relpath);
    int (*mkdir)(struct inode *mountpt, const char *relpath, mode_t mode);
    int (*rmdir)(struct inode *mountpt, const char *relpath);
    int (*rename)(struct inode *mountpt, const char *oldrelpath, const char *newrelpath);
    int (*stat)(struct inode *mountpt, const char *relpath, struct stat *buf);
    int (*chstat)(struct inode *mountpt, const char *relpath, const struct stat *buf, int flags);

    ssize_t (*readlink)(struct inode *mountpt, const char *relpath, char *buf, size_t bufsize);
    int (*symlink)(struct inode *mountpt, const char *target, const char *link_relpath);
    int (*link)(struct inode *mountpt, const char *target, const char *link_relpath);
};
#endif

union inode_ops_u {
    const struct file_operations *i_ops;
#ifndef CONFIG_DISABLE_MOUNTPOINT
    const struct block_operations *i_bops;
    struct mtd_dev_s *i_mtd;
    const struct mountpt_operations *i_mops;
#endif
#ifdef CONFIG_FS_NAMED_SEMAPHORES
    struct nsem_inode_s *i_nsem;
#endif
#ifdef CONFIG_PSEUDOFS_SOFTLINKS
    char *i_link;
#endif
};

struct inode
{
    struct inode *i_parent;
    struct inode *i_peer;
    struct inode *i_child;
    int16_t i_crefs;
    uint16_t i_flags;
    union inode_ops_u u;
    ino_t i_ino;
    dev_t dev;

    size_t i_size;

#ifdef CONFIG_PSEUDOFS_ATTRIBUTES
    mode_t i_mode;
    uid_t i_owner;
    gid_t i_group;
    struct timespec i_atime;
    struct timespec i_mtime;
    struct timespec i_ctime;
#endif
    void *i_private;
    char i_name[1];
};

#define FSNODE_SIZE(n) (sizeof(struct inode) + (n))

struct file
{
    int f_oflags;
    off_t f_pos;
    struct inode *f_inode;
    void *f_priv;
};

typedef struct T_TTOS_SemaControlBlock_Struct *MUTEX_ID;
struct filelist
{
    MUTEX_ID fl_lock;
    uint8_t fl_rows;
    struct file **fl_files;
};

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

void foreach_bd_filesystems(void (*func)(const char *filesystemtype, void *arg), void *arg);
void foreach_nonbd_filesystems(void (*func)(const char *filesystemtype, void *arg), void *arg);

void fs_initialize(void);

struct filelist *sched_get_files(void);

int files_foreach(struct filelist *list, void (*f)(int fd, void *ctx), void *ctx);

int vfs_fstat(int fd, struct stat *buf);

mode_t vfs_getumask(void);

int vfs_mkdir(const char *pathname, mode_t mode);

int vfs_statfs(const char *path, struct statfs *buf);
int vfs_fstatfs(int fd, struct statfs *buf);

int vfs_rmdir(const char *pathname);

#ifdef CONFIG_PSEUDOFS_SOFTLINKS
int vfs_symlink(const char *path1, const char *path2);
int vfs_link(const char *path1, const char *path2);
#endif

int vfs_rename(const char *oldpath, const char *newpath);

int vfs_access(const char *path, int amode);

mode_t vfs_umask(mode_t mask);

int device_bind_path(struct device *device, const char *path, const struct file_operations *fops,
                     mode_t mode);
int vfs_bind_path(const char *path, const struct file_operations *fops, dev_t dev, mode_t mode,
                  void *data);
int register_driver(const char *path, const struct file_operations *fops, mode_t mode, void *priv);

#ifndef CONFIG_DISABLE_MOUNTPOINT
int register_blockdriver(const char *path, const struct block_operations *bops, mode_t mode,
                         void *priv);
#endif

#ifndef CONFIG_DISABLE_MOUNTPOINT
int register_blockpartition(const char *partition, mode_t mode, const char *parent,
                            off_t firstsector, off_t nsectors);
#endif

int unregister_driver(const char *path);

int unregister_blockdriver(const char *path);

#ifdef CONFIG_MTD
int register_mtddriver(const char *path, struct mtd_dev_s *mtd, mode_t mode, void *priv);
#endif

#ifdef CONFIG_MTD
int register_mtdpartition(const char *partition, mode_t mode, const char *parent, off_t firstblock,
                          off_t nblocks);
#endif

#ifdef CONFIG_MTD
int unregister_mtddriver(const char *path);
#endif

#ifndef CONFIG_DISABLE_MOUNTPOINT
int vfs_mount(const char *source, const char *target, const char *filesystemtype,
              unsigned long mountflags, const void *data);
#endif

#ifndef CONFIG_DISABLE_MOUNTPOINT
int vfs_umount2(const char *target, unsigned int flags);
#endif

void files_initlist(struct filelist *list);

void files_releaselist(struct filelist *list);

int files_stdinout_duplist(struct filelist *plist, struct filelist *clist);
int files_duplist(struct filelist *plist, struct filelist *clist);
int files_close_on_exec(struct filelist *clist);

int file_dup(struct file *filep, int minfd);

int vfs_dup(int fd);

int file_dup2(struct file *filep1, struct file *filep2);

int vfs_dup2(int fd1, int fd2);

int file_open(struct file *filep, const char *path, int oflags, ...);

int vfs_open(const char *path, int oflags, ...);

int fs_getfilep(int fd, struct file **filep);
int kernel_fs_getfilep(int fd, struct file **filep);

int fs_getfilep_by_list(int fd, struct file **filep, struct filelist *list);

int file_close(struct file *filep);

int vfs_close(int fd);

int open_blockdriver(const char *pathname, int mountflags, struct inode **ppinode);

int close_blockdriver(struct inode *inode);

int find_mtddriver(const char *pathname, struct inode **ppinode);

int close_mtddriver(struct inode *pinode);

ssize_t file_read(struct file *filep, void *buf, size_t nbytes);

ssize_t vfs_read(int fd, void *buf, size_t nbytes);

ssize_t file_write(struct file *filep, const void *buf, size_t nbytes);

ssize_t vfs_write(int fd, const void *buf, size_t nbytes);

ssize_t vfs_pread(int fd, void *buf, size_t nbytes, off_t offset);
ssize_t file_pread(struct file *filep, void *buf, size_t nbytes, off_t offset);

ssize_t vfs_pwrite(int fd, const void *buf, size_t nbytes, off_t offset);
ssize_t file_pwrite(struct file *filep, const void *buf, size_t nbytes, off_t offset);

ssize_t file_sendfile(struct file *outfile, struct file *infile, off_t *offset, size_t count);

off_t file_seek(struct file *filep, off_t offset, int whence);

off_t vfs_seek(int fd, off_t offset, int whence);

off_t vfs_lseek(int fd, off_t offset, int whence);

ssize_t vfs_readlink(const char *path, char *buf, size_t bufsize);

int file_fsync(struct file *filep);

int vfs_fsync(int fd);

#ifndef CONFIG_DISABLE_MOUNTPOINT
int file_truncate(struct file *filep, off_t length);
#endif

int vfs_ftruncate(int fd, off_t length);

int file_mmap(struct file *filep, void *start, size_t length, int prot, int flags, off_t offset,
              void **mapped);

void *vfs_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset,
               bool iskernel);

int file_munmap(void *start, size_t length);

int vfs_munmap(void *start, size_t length);

int file_ioctl(struct file *filep, unsigned int req, ...);

int vfs_ioctl(int fd, unsigned int req, ...);

int file_fcntl(struct file *filep, unsigned int cmd, ...);

int vfs_fcntl(int fd, unsigned int cmd, ...);

int file_poll(struct file *filep, struct kpollfd *fds, bool setup);

int nx_poll(struct pollfd *fds, unsigned int nfds, int timeout);

int file_fstat(struct file *filep, struct stat *buf);

int vfs_stat(const char *path, struct stat *buf, int resolve);

int vfs_fchmod(int fd, mode_t mode);
int vfs_fchown(int fd, uid_t owner, gid_t group);
int vfs_lchown(const char *path, uid_t owner, gid_t group);
int vfs_chown(const char *path, uid_t owner, gid_t group);
int vfs_chmod(const char *path, mode_t mode);
int vfs_utimens(const char *path, const struct timespec times[2]);
int file_fchstat(struct file *filep, struct stat *buf, int flags);

int vfs_unlink(const char *pathname);

int file_pipe(struct file *filep[2], size_t bufsize, int flags);
int vfs_pipe2(int fd[2], int flags);

#if defined(CONFIG_PIPES) && CONFIG_DEV_FIFO_SIZE > 0
int nx_mkfifo(const char *pathname, mode_t mode, size_t bufsize);
#endif

int vfs_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
               struct timeval *timeout);

/****************************************************************************
 * Name: vfs_epoll_create
 *
 * Description:
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

int vfs_epoll_create(int size);

/****************************************************************************
 * Name: vfs_epoll_create1
 *
 * Description:
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

int vfs_epoll_create1(int flags);

/****************************************************************************
 * Name: vfs_epoll_close
 *
 * Description:
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

void vfs_epoll_close(int epfd);

/****************************************************************************
 * Name: vfs_epoll_ctl
 *
 * Description:
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/
struct epoll_event;
int vfs_epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev);

/****************************************************************************
 * Name: vfs_epoll_pwait
 ****************************************************************************/

int vfs_epoll_pwait(int epfd, struct epoll_event *evs, int maxevents, int timeout,
                    const k_sigset_t *sigmask);

/****************************************************************************
 * Name: vfs_epoll_wait
 *
 * Description:
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

int vfs_epoll_wait(int epfd, struct epoll_event *evs, int maxevents, int timeout);

#ifdef CONFIG_EVENT_FD
int vfs_eventfd(unsigned int count, int flags);
#endif

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif
