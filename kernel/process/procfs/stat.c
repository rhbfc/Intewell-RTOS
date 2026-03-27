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

#include <assert.h>
#include <dirent.h>
#include <driver/cpudev.h>
#include <errno.h>
#include <fnmatch.h>
#include <fs/fs.h>
#include <fs/procfs.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <time/ktime.h>
#include <ttosProcess.h>
#include <ttos_init.h>

#define KLOG_TAG "PROCFS"
#include <klog.h>

struct seq_data
{
    char buf[256];
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
static int proc_file_release(struct proc_dir_entry *inode, void *buffer)
{
    struct seq_data *data = buffer;
    if (data)
    {
        free(data);
    }
    return 0;
}

static ssize_t proc_file_read(struct proc_dir_entry *inode, void *priv, char *buffer, size_t count,
                              off_t *ppos)
{
    struct seq_data *data = (struct seq_data *)priv;
    struct cpudev *cpu;

    if (*ppos == 0)
    {
        /* 初始化序列化缓冲区 */
        proc_seq_init(&data->seq, data->buf, sizeof(data->buf), ppos);

        struct timespec64 stime, utime, itime;

        stime = (struct timespec64){0ULL, 0ULL};
        utime = (struct timespec64){0ULL, 0ULL};
        itime = (struct timespec64){0ULL, 0ULL};
        {
            for_each_present_cpu(cpu)
            {
                if (likely(cpu->state == CPU_STATE_RUNNING))
                {
                    stime = clock_timespec_add64(&stime, &cpu->stime);
                    utime = clock_timespec_add64(&utime, &cpu->utime);
                    itime = clock_timespec_add64(&itime, &cpu->itime);
                }
            }
        }
        proc_seq_printf(&data->seq, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu\n",
                        (unsigned long long)timespec64_to_clock_t(&utime),
                        0ULL, // nice
                        (unsigned long long)timespec64_to_clock_t(&stime),
                        (unsigned long long)timespec64_to_clock_t(&itime),
                        0ULL, // iowait
                        0ULL, // irq
                        0ULL, // softirq
                        0ULL, // steal
                        0ULL  // guest
        );
        for_each_present_cpu(cpu)
        {
            if (likely(cpu->state == CPU_STATE_RUNNING))
            {
                proc_seq_printf(&data->seq, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu\n",
                                cpu->index, (unsigned long long)timespec64_to_clock_t(&cpu->utime),
                                0ULL, // nice
                                (unsigned long long)timespec64_to_clock_t(&cpu->stime),
                                (unsigned long long)timespec64_to_clock_t(&cpu->itime),
                                0ULL, // iowait
                                0ULL, // irq
                                0ULL, // softirq
                                0ULL, // steal
                                0ULL  // guest
                );
            }
        }
    }

    return proc_seq_read(&data->seq, buffer, count);
}

static struct proc_ops ops = {
    .proc_open = proc_file_open, .proc_read = proc_file_read, .proc_release = proc_file_release};

static int procfs_stat_init(void)
{
    proc_create("stat", 0444, NULL, &ops);
    return 0;
}
INIT_EXPORT_SERVE_FS(procfs_stat_init, "register /proc/stat");
