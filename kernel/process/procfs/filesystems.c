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
#include <fs/procfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <ttos_init.h>

struct seq_data
{
    char buf[4096];
    struct proc_seq_buf seq;
};

struct mount_ctx
{
    struct seq_data *seq;
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

static void nonbd_filesystems_enum(const char *filesystemtype, void *arg)
{
    struct proc_seq_buf *seq = arg;
    proc_seq_printf(seq, "nodev   %s\n", filesystemtype);
}

static void bd_filesystems_enum(const char *filesystemtype, void *arg)
{
    struct proc_seq_buf *seq = arg;
    proc_seq_printf(seq, "        %s\n", filesystemtype);
}

static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        foreach_bd_filesystems(bd_filesystems_enum, &data->seq);
        foreach_nonbd_filesystems(nonbd_filesystems_enum, &data->seq);
    }

    return proc_seq_read(&data->seq, buffer, count);
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
static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};
static int procfs_process_filesystems_init(void)
{
    proc_create("filesystems", 0444, NULL, &ops);
    return 0;
}
INIT_EXPORT_SERVE_FS(procfs_process_filesystems_init, "register /proc/filesystems");
