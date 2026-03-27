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

#include <ttos_init.h>

#ifdef CONFIG_FS_PROCFS
#include <fs/procfs.h>
#include <klog.h>

static int proc_file_open(struct proc_dir_entry *inode, void *buffer)
{
    return 0;
}
static int proc_file_release(struct proc_dir_entry *inode, void *buffer)
{
    return 0;
}
static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    unsigned int pos = *ppos;
    ssize_t len = klog_read_msg(buffer, &pos, count, false);
    *ppos = pos;
    return len;
}

ssize_t proc_get_size(struct proc_dir_entry *entry)
{
    return klog_get_size();
}

static struct proc_ops ops = {
    .proc_open = proc_file_open,
    .proc_read = proc_file_read,
    .proc_release = proc_file_release,
    .proc_get_size = proc_get_size,
};

static int procfs_klog_init(void)
{
    proc_create("kmsg", 0444, NULL, &ops);
    proc_symlink("klog", NULL, "kmsg");
    return 0;
}
INIT_EXPORT_SUBSYS(procfs_klog_init, "procfs klog init");
#endif