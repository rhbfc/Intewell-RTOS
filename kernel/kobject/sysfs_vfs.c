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

#include <errno.h>
#include <fs/fs.h>
#include <kobject.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sysfs.h>
#include <ttos_init.h>

#define KLOG_TAG "sysfs"
#include <klog.h>

struct sysfs_open_file
{
    struct kobject *kobj;
    char *path;
    int refs;
};

struct sysfs_open_dir
{
    struct fs_dirent_s base;
    struct kobject *kobj;
    int offset;
};

static int sysfs_vfs_open(struct file *filep, const char *relpath, int oflags, mode_t mode);
static int sysfs_vfs_close(struct file *filep);
static ssize_t sysfs_vfs_read(struct file *filep, char *buffer, size_t buflen);
static ssize_t sysfs_vfs_write(struct file *filep, const char *buffer, size_t buflen);
static off_t sysfs_vfs_seek(struct file *filep, off_t offset, int whence);
static int sysfs_vfs_ioctl(struct file *filep, int cmd, unsigned long arg);
static int sysfs_vfs_sync(struct file *filep);
static int sysfs_vfs_dup(const struct file *oldp, struct file *newp);
static int sysfs_vfs_fstat(const struct file *filep, struct stat *buf);
static int sysfs_vfs_truncate(struct file *filep, off_t length);
static int sysfs_vfs_opendir(struct inode *mountpt, const char *relpath, struct fs_dirent_s **dir);
static int sysfs_vfs_closedir(struct inode *mountpt, struct fs_dirent_s *dir);
static int sysfs_vfs_readdir(struct inode *mountpt, struct fs_dirent_s *dir, struct dirent *entry);
static int sysfs_vfs_rewinddir(struct inode *mountpt, struct fs_dirent_s *dir);
static int sysfs_vfs_bind(struct inode *driver, const void *data, void **handle);
static int sysfs_vfs_unbind(void *handle, struct inode **driver, unsigned int flags);
static int sysfs_vfs_statfs(struct inode *mountpt, struct statfs *buf);
static int sysfs_vfs_stat(struct inode *mountpt, const char *relpath, struct stat *buf);

const struct mountpt_operations sysfs_operations = {
    sysfs_vfs_open,
    sysfs_vfs_close,
    sysfs_vfs_read,
    sysfs_vfs_write,
    sysfs_vfs_seek,
    sysfs_vfs_ioctl,
    NULL,
    NULL,
    sysfs_vfs_sync,
    sysfs_vfs_dup,
    sysfs_vfs_fstat,
    NULL,
    sysfs_vfs_truncate,
    sysfs_vfs_opendir,
    sysfs_vfs_closedir,
    sysfs_vfs_readdir,
    sysfs_vfs_rewinddir,
    sysfs_vfs_bind,
    sysfs_vfs_unbind,
    sysfs_vfs_statfs,
    NULL,
    NULL,
    NULL,
    NULL,
    sysfs_vfs_stat,
    NULL,
    NULL,
    NULL,
    NULL,
};

static int sysfs_vfs_open(struct file *filep, const char *relpath, int oflags, mode_t mode)
{
    struct sysfs_open_file *handle;

    (void)oflags;
    (void)mode;

    handle = malloc(sizeof(*handle));
    if (!handle)
        return -ENOMEM;

    handle->path = strdup(relpath ? relpath : "");
    if (!handle->path)
    {
        free(handle);
        return -ENOMEM;
    }

    handle->kobj = NULL;
    handle->refs = 1;
    filep->f_priv = handle;
    return 0;
}

static int sysfs_vfs_close(struct file *filep)
{
    struct sysfs_open_file *handle = (struct sysfs_open_file *)filep->f_priv;

    if (!handle)
        return -EINVAL;

    handle->refs--;
    if (handle->refs == 0)
    {
        free(handle->path);
        free(handle);
    }

    return 0;
}

static ssize_t sysfs_vfs_read(struct file *filep, char *buffer, size_t buflen)
{
    (void)filep;
    (void)buffer;
    (void)buflen;
    return -EACCES;
}

static ssize_t sysfs_vfs_write(struct file *filep, const char *buffer, size_t buflen)
{
    (void)filep;
    (void)buffer;
    (void)buflen;
    return -EACCES;
}

static off_t sysfs_vfs_seek(struct file *filep, off_t offset, int whence)
{
    (void)filep;
    (void)offset;
    (void)whence;
    return -EACCES;
}

static int sysfs_vfs_ioctl(struct file *filep, int cmd, unsigned long arg)
{
    (void)filep;
    (void)cmd;
    (void)arg;
    return -ENOTTY;
}

static int sysfs_vfs_sync(struct file *filep)
{
    (void)filep;
    return 0;
}

static int sysfs_vfs_dup(const struct file *oldp, struct file *newp)
{
    struct sysfs_open_file *handle = (struct sysfs_open_file *)oldp->f_priv;

    if (!handle)
        return -EINVAL;

    handle->refs++;
    newp->f_priv = handle;
    return 0;
}

static int sysfs_vfs_fstat(const struct file *filep, struct stat *buf)
{
    (void)filep;

    if (!buf)
        return -EINVAL;

    memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFREG | 0444;
    buf->st_size = 4096;
    buf->st_blksize = 4096;
    return 0;
}

static int sysfs_vfs_truncate(struct file *filep, off_t length)
{
    (void)filep;
    (void)length;
    return -EACCES;
}

static int sysfs_vfs_opendir(struct inode *mountpt, const char *relpath, struct fs_dirent_s **dir)
{
    struct sysfs_open_dir *handle;

    (void)mountpt;
    (void)relpath;

    handle = malloc(sizeof(*handle));
    if (!handle)
        return -ENOMEM;

    memset(handle, 0, sizeof(*handle));
    *dir = &handle->base;
    return 0;
}

static int sysfs_vfs_closedir(struct inode *mountpt, struct fs_dirent_s *dir)
{
    (void)mountpt;
    free(dir);
    return 0;
}

static int sysfs_vfs_readdir(struct inode *mountpt, struct fs_dirent_s *dir, struct dirent *entry)
{
    (void)mountpt;
    (void)dir;
    (void)entry;
    return -ENOENT;
}

static int sysfs_vfs_rewinddir(struct inode *mountpt, struct fs_dirent_s *dir)
{
    struct sysfs_open_dir *handle = (struct sysfs_open_dir *)dir;

    (void)mountpt;
    if (handle)
        handle->offset = 0;

    return 0;
}

static int sysfs_vfs_bind(struct inode *driver, const void *data, void **handle)
{
    (void)driver;
    (void)data;
    *handle = NULL;
    return 0;
}

static int sysfs_vfs_unbind(void *handle, struct inode **driver, unsigned int flags)
{
    (void)handle;
    (void)driver;
    (void)flags;
    return 0;
}

static int sysfs_vfs_statfs(struct inode *mountpt, struct statfs *buf)
{
    (void)mountpt;

    if (!buf)
        return -EINVAL;

    memset(buf, 0, sizeof(*buf));
    buf->f_type = 0x62656572;
    buf->f_bsize = 4096;
    buf->f_files = 1000;
    buf->f_namelen = 255;
    return 0;
}

static int sysfs_vfs_stat(struct inode *mountpt, const char *relpath, struct stat *buf)
{
    (void)mountpt;
    (void)relpath;

    if (!buf)
        return -EINVAL;

    memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFDIR | 0755;
    buf->st_size = 4096;
    buf->st_blksize = 4096;
    buf->st_nlink = 2;
    return 0;
}

static int sysfs_mount_init(void)
{
    int ret;

    ret = vfs_mount(NULL, "/sys", "sysfs", 0, NULL);
    if (ret == 0)
        KLOG_I("sysfs mounted at /sys");
    else
        KLOG_W("sysfs mount returned %d", ret);

    return ret;
}

INIT_EXPORT_SERVE_FS(sysfs_mount_init, "sysfs mount");
