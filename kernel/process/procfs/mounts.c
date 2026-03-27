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

#include <fs/procfs.h>
#include <stdio.h>
#include <time/ktime.h>
#include <ttosProcess.h>
#include <ttos_init.h>

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

static void mountpoint_enum(const char *mount_path, const char *source, const char *fstype,
                            unsigned long mountflags, void *arg)
{
    struct proc_seq_buf *seq = arg;
    proc_seq_printf(seq, "%s %s %s %s 0 0\n", source, mount_path, fstype,
                    (strcmp(source, fstype) == 0) ? "rw,nosuid,nodev,noexec,relatime"
                                                  : "rw,relatime");
}

void foreach_mount_point(void (*func)(const char *mount_path, const char *source,
                                      const char *fstype, unsigned long mountflags, void *arg),
                         void *arg);
static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        foreach_mount_point(mountpoint_enum, &data->seq);
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
static int procfs_process_mounts_init(void)
{
    proc_create("mounts", 0444, NULL, &ops);
    return 0;
}
INIT_EXPORT_SERVE_FS(procfs_process_mounts_init, "register /proc/mounts");
