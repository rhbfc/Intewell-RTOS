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

#ifndef PROCFS_H
#define PROCFS_H

#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* 前向声明 */
struct proc_dir_entry;

/* 判断目录项类型的宏 */
#define IS_PROCFS_DIR(entry) S_ISDIR((entry)->mode)
#define IS_PROCFS_REG(entry) S_ISREG((entry)->mode)
#define IS_PROCFS_LNK(entry) S_ISLNK((entry)->mode)

/* 目录项标记位 */
#define PROC_ENTRY_DYNAMIC 0x00000001 /* 动态生成的目录项 */

/* 判断目录项是否为动态生成 */
#define IS_DYNAMIC_ENTRY(entry) ((entry)->flags & PROC_ENTRY_DYNAMIC)

/* procfs文件操作结构体 */
struct proc_ops
{
    ssize_t (*proc_get_size)(struct proc_dir_entry *);
    int (*proc_open)(struct proc_dir_entry *, void *);
    ssize_t (*proc_read)(struct proc_dir_entry *, void *, char *, size_t, off_t *);
    ssize_t (*proc_write)(struct proc_dir_entry *, void *, const char *, size_t, off_t *);
    int (*proc_release)(struct proc_dir_entry *, void *);
};

/* procfs目录项结构体 */
struct proc_dir_entry
{
    char *name;
    mode_t mode;
    off_t size;
    char *link;
    void *data;
    uid_t uid;
    gid_t gid;
    pid_t pid;
    ino_t inode;
    struct proc_dir_entry *parent;
    struct proc_dir_entry *next;
    struct proc_dir_entry *subdir;
    const struct proc_ops *proc_ops;
    const char *(*get_link)(struct proc_dir_entry *, char *);
    /* 动态目录支持 */
    int (*find_sub_callback)(struct proc_dir_entry *, struct proc_dir_entry *, const char *);
    int (*readdir_callback)(struct proc_dir_entry *, struct proc_dir_entry *, void **);
    int (*release_callback)(struct proc_dir_entry *, void *);
    void *readdir_data;
    /* 标记位 */
    unsigned int flags;
};

/* 文件系统操作接口 */
struct procfs_file
{
    struct proc_dir_entry *entry; /* 关联的目录项 */
    void *private_data;           /* 私有数据 */
    int flags;                    /* 打开标志 */
    off_t pos;                    /* 文件位置 */
};

/* 目录迭代器 */
struct procfs_dir
{
    struct proc_dir_entry *entry;   /* 当前目录 */
    struct proc_dir_entry *current; /* 当前遍历到的项 */
    void *iter_data;                /* 动态目录迭代状态 */
};

/* 字符串序列化缓冲区结构体 */
struct proc_seq_buf
{
    char *buf;    /* 缓冲区指针 */
    size_t size;  /* 缓冲区大小 */
    size_t count; /* 当前使用的大小 */
    off_t *ppos;  /* 文件位置指针 */
    int overflow; /* 溢出标志 */
};

/* 字符串序列化相关函数 */
int proc_seq_init(struct proc_seq_buf *seq, char *buf, size_t size, off_t *ppos);
int proc_seq_printf(struct proc_seq_buf *seq, const char *fmt, ...);
ssize_t proc_seq_read(struct proc_seq_buf *seq, char *buf, size_t size);
int proc_seq_putc(struct proc_seq_buf *seq, const char chr);
int proc_seq_write(struct proc_seq_buf *seq, const char *buff, size_t size);

/* 创建procfs根目录 */
struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *parent);

/* 创建procfs文件 */
struct proc_dir_entry *proc_create(const char *name, mode_t mode, struct proc_dir_entry *parent,
                                   const struct proc_ops *proc_ops);

/* 删除procfs文件或目录 */
void proc_remove(struct proc_dir_entry *entry);
void proc_destory(const char *name);

/* 清理procfs */
void proc_cleanup(void);

/* 文件系统操作接口 */
int procfs_open(const char *path, int flags, struct procfs_file **file);
int procfs_close(struct procfs_file *file);
ssize_t procfs_read(struct procfs_file *file, char *buf, size_t size);
ssize_t procfs_write(struct procfs_file *file, const char *buf, size_t size);
off_t procfs_seek(struct procfs_file *file, off_t offset, int whence);

/* 符号链接操作接口 */
int procfs_readlink(const char *path, char *buf, size_t size);

/* 目录操作接口 */
int procfs_opendir(const char *path, struct procfs_dir **dir);
int procfs_closedir(struct procfs_dir *dir);
struct proc_dir_entry *procfs_readdir(struct procfs_dir *dir);
int procfs_rewinddir(struct procfs_dir *dir);
int procfs_stat(const char *path, struct stat *st);
int procfs_lstat(const char *path, struct stat *st);
int procfs_fstat(struct procfs_file *file, struct stat *buf);

/* 添加符号链接相关函数声明 */
struct proc_dir_entry *proc_symlink(const char *name, struct proc_dir_entry *parent,
                                    const char *target);
struct proc_dir_entry *proc_dynamic_link(const char *name, struct proc_dir_entry *parent,
                                         const char *(*get_link)(struct proc_dir_entry *, char *));

/* 动态目录相关函数声明 */
struct proc_dir_entry *proc_dynamic_dir(
    const char *name, struct proc_dir_entry *parent,
    int (*find_sub_callback)(struct proc_dir_entry *, struct proc_dir_entry *, const char *),
    int (*readdir_callback)(struct proc_dir_entry *, struct proc_dir_entry *, void **),
    int (*release_callback)(struct proc_dir_entry *, void *), void *data);

/* 填充动态目录项 */
int dynamic_entry_fill(struct proc_dir_entry *entry, const char *name, mode_t mode);
struct proc_dir_entry *find_entry_by_path(const char *path);

#endif /* PROCFS_H */