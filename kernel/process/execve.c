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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pgroup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>
#include <sys/stat.h>
#include <tglock.h>
#include <trace.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <unistd.h>

#define KLOG_TAG "EXECVE"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

__noreturn void arch_run2user(void);

static void init_entry(void *param)
{
    /* 切换空间 */
    mm_switch_space_to(get_process_mm(ttosProcessSelf()));

    /* 进入用户态 */
    arch_run2user();
}

int load_elf(struct file *f, pcb_t pcb, const char *argv[], const char *envp[]);
int check_exec(const char *path);

static char *build_cmdline(char **argv)
{
    char *cmdline;
    int total_len = 0;
    int i = 0;
    while (argv[i] != NULL)
    {
        total_len += strlen(argv[i]) + 1;
        i++;
    }
    if (total_len == 0)
    {
        return strdup("");
    }

    cmdline = malloc(total_len);
    if (cmdline == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    cmdline[0] = '\0';
    i = 0;
    while (argv[i] != NULL)
    {
        strlcat(cmdline, argv[i], total_len);
        strlcat(cmdline, " ", total_len);
        i++;
    }
    return cmdline;
}

static const char *const *user_strings_dup(const char *runner_argv0,
                                           const char __user *const __user *strv)
{
    int len = !!(runner_argv0);
    char **kstrv = NULL;
    const char __user *const __user *ustrv = strv;
    if (strv == NULL)
    {
        return calloc(1, sizeof(char *));
    }
    while (*ustrv++ != NULL)
    {
        len++;
    }
    kstrv = calloc(len + 1, sizeof(char *));

    if (runner_argv0 != NULL)
    {
        *kstrv++ = strdup(runner_argv0);
    }

    ustrv = strv;
    while (*ustrv != NULL)
    {
        *kstrv++ = strdup(*ustrv++);
    }
    return (const char *const *)(kstrv - len);
}

static void strings_free(const char *const *strv)
{
    const char *const *kstrv = strv;
    while (*kstrv != NULL)
    {
        free((void *)*kstrv++);
    }
    free((void *)strv);
}

pid_t kernel_execve(const char *filename, const char *const *argv, const char *const *envp)
{
    pcb_t pcb;
    int ret, fd;
    long flags;
    TASK_ID task = NULL;
    T_TTOS_ReturnCode ttos_ret;
    char *cmdline;
    pgroup_t group = NULL;
    struct file f;
    irq_flags_t irq_flag = 0;

    ret = check_exec(filename);
    if (ret < 0)
    {
        return ret;
    }

    T_TTOS_ConfigTask taskParam = {"\0",
                                   225,
                                   TRUE,
                                   TRUE,
                                   init_entry,
                                   NULL,
                                   TTOS_DEFAULT_TASK_SLICE_SIZE,
                                   CONFIG_KERNEL_PROCESS_STACKSIZE + ttosGetPageSize(),
                                   TTOS_SCHED_NONPERIOD,
                                   0,
                                   0,
                                   0,
                                   0,
                                   TTOS_OBJ_CONFIG_CURRENT_VERSION,
                                   TRUE,
                                   TRUE};

    strncpy((char *)taskParam.cfgTaskName, (const char *)argv[0], sizeof(taskParam.cfgTaskName));
    taskParam.taskStack =
        (T_UBYTE *)memalign(ttosGetPageSize(), CONFIG_KERNEL_PROCESS_STACKSIZE + ttosGetPageSize());
    taskParam.isprocess = TRUE;

    pcb = calloc(1, sizeof(struct T_TTOS_ProcessControlBlock));
    if (pcb == NULL)
    {
        return -ENOMEM;
    }

    taskParam.pcb = pcb;
    pcb->obj_list = LIST_HEAD_INIT(pcb->obj_list);
    /* 创建pcb自旋锁 */
    INIT_SPIN_LOCK(&pcb->lock);
    tg_lock_create(pcb);

    /* 创建新的 pwd */
    strncpy(pcb->pwd, "/", sizeof(pcb->pwd));

    for (int i = 0;; i++)
    {
        if (envp[i] == NULL)
        {
            break;
        }
        ret = sscanf(envp[i], "PWD=%s", pcb->pwd);
        if (ret == 1)
        {
            break;
        }
    }
    strcpy(pcb->root, "/");
    pcb->clear_child_tid = 0;
    pcb->pid = pid_obj_alloc(pcb);
    if (pcb->pid == NULL)
    {
        KLOG_E("%s %d:alloc pid failed", __func__, __LINE__);
    }
    /* sid 暂未使用 */
    pcb->sid = 0;

    /* 配置启动用户为root:root */
    pcb->uid = 0;
    pcb->euid = 0;
    pcb->suid = 0;

    pcb->gid = 0;
    pcb->egid = 0;
    pcb->sgid = 0;

    pcb->umask = 0;

    process_wait_info_create(pcb);
    /* 创建空的内存管理块 */
    process_mm_create(pcb);
    /* 创建filelist */
    process_filelist_create(pcb);
    /* dup stdin stdout */
    files_stdinout_duplist(pcb_get_files(NULL), pcb_get_files(pcb));

    strncpy(pcb->cmd_name, basename((char *)filename), sizeof pcb->cmd_name);
    strncpy(pcb->cmd_path, filename, sizeof pcb->cmd_name);

    /* 设置ASID */
    get_process_mm(pcb)->asid = get_process_pid(pcb);

    /* 切换空间 */
    mm_switch_space_to(get_process_mm(pcb));

    INIT_LIST_HEAD(&(pcb->thread_group));

    pcb->tgid = get_process_pid(pcb);
    pcb->group_leader = pcb;
    tg_lock(pcb->group_leader, &irq_flag);
    list_add(&pcb->sibling_node, &pcb->thread_group);
    tg_unlock(pcb->group_leader, &irq_flag);

    ret = fork_signal(0, pcb);
    if (ret)
    {
        /*
         * 当前先保留原有执行路径，只把 signal 初始化失败明确打到日志里，
         * 方便后续在完整编译/回归环境下继续补 execve 早期失败回滚。
         */
        KLOG_E("fork_signal failed during kernel_execve, pcb=%p path=%s ret=%d",
               pcb, pcb->cmd_path, ret);
    }

    group = pgrp_create(pcb);
    if (group)
    {
        pgrp_insert(group, pcb);
        pgrp_put(group);
    }
    ret = file_open(&f, pcb->cmd_path, O_RDONLY);

    if (ret < 0)
    {
        KLOG_E("%s : %s", pcb->cmd_path, strerror(-ret));
        goto failed;
    }

    ret = load_elf(&f, pcb, (const char **)argv, (const char **)envp);
    file_close(&f);

    if (ret < 0)
    {
        KLOG_E("elf load error");
        goto failed;
    }

    struct mm *kernel_mm = get_kernel_mm();
    mm_switch_space_to(kernel_mm);

    pcb->envp = process_obj_create(pcb, (void *)user_strings_dup(NULL, envp),
                                   (void (*)(void *))strings_free);

    cmdline = build_cmdline((char **)argv);
    pcb->cmdline = process_obj_create(pcb, cmdline, free);

    ttos_ret = TTOS_CreateTask(&taskParam, &task);
    if (ttos_ret != TTOS_OK)
    {
        free(taskParam.taskStack);
        KLOG_E("TTOS_CreateTaskEx failed: %d", ttos_ret);
        return -ENOMEM;
    }

    // TTOS_ActiveTask(task);

    return get_process_pid(pcb);
failed:
    if (pcb)
    {
        process_destroy(pcb);
    }
    return -1;
}

static char *readline(struct file *f, char *buf, int size)
{
    int ret;
    char c;
    int i = 0;
    while (i < size - 1)
    {
        ret = file_read(f, &c, 1);
        if (ret <= 0)
        {
            break;
        }
        if (c == '\n')
        {
            break;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return buf;
}

char *get_runner_if_script(const char *path)
{
    int ret;
    struct file f;
    char buf[PATH_MAX];
    if (access(path, X_OK) != 0)
    {
        return NULL;
    }
    ret = file_open(&f, path, O_RDONLY);
    if (ret < 0)
    {
        return NULL;
    }
    ret = file_read(&f, buf, 2);

    if (ret != 2)
    {
        file_close(&f);
        return NULL;
    }

    if (buf[0] == '#' && buf[1] == '!')
    {
        char *line = readline(&f, buf, sizeof(buf));
        file_close(&f);
        while (isspace(*line))
        {
            line++;
        }
        return strdup(line);
    }
    file_close(&f);
    return NULL;
}

int do_execve(const char __user *filename, const char __user *const __user *argv,
              const char __user *const __user *envp)
{
    const char *const *kargv;
    const char *const *kenvp;
    pcb_t pcb = ttosProcessSelf();
    char *fullpath = process_getfullpath(AT_FDCWD, filename);
    int ret;
    char *runner_path = NULL;
    struct stat st;
    struct file f;

    char *linkpath = malloc(PATH_MAX);
    if (linkpath == NULL)
    {
        return -ENOMEM;
    }
    if (realpath(fullpath, linkpath) == NULL)
    {
        free(linkpath);
        return -errno;
    }
    ret = check_exec(linkpath);
    if (ret < 0)
    {
        runner_path = get_runner_if_script(linkpath);
        if (runner_path == NULL)
        {
            free(linkpath);
            free(fullpath);
            return ret;
        }
    }

    assert(pcb != NULL);

    kargv = user_strings_dup(runner_path ? basename(runner_path) : NULL, argv);

    if (runner_path)
    {
        free((void *)kargv[1]);
        ((char **)kargv)[1] = strdup(linkpath);
    }

    kenvp = user_strings_dup(NULL, envp);

    if (runner_path != NULL)
    {
        if (realpath(runner_path, linkpath) == NULL)
        {
            free(linkpath);
            free(runner_path);
            strings_free(kargv);
            strings_free(kenvp);
            return -errno;
        }
        free(runner_path);
        runner_path = NULL;

        ret = check_exec(linkpath);
        if (ret < 0)
        {
            strings_free(kargv);
            strings_free(kenvp);
            free(linkpath);
            return ret;
        }
    }

    strncpy(pcb->cmd_name, basename((char *)filename), sizeof pcb->cmd_name);
    strncpy(pcb->cmd_path, linkpath, sizeof pcb->cmd_path);

    free(fullpath);
    free(linkpath);
    linkpath = NULL;
    fullpath = NULL;

    TTOS_SetTaskName(pcb->taskControlId, (T_UBYTE *)filename);

    char *cmdline = build_cmdline((char **)argv);
    process_obj_destroy(pcb->cmdline);
    pcb->cmdline = process_obj_create(pcb, cmdline, free);

    KLOG_D("execve, pcb %p, task name ---> %s", pcb, cmdline);

    /* 创建新的mmu空间 */
    process_obj_destroy(pcb->mm);
    process_mm_create(pcb);

    pcb_flags_clear(pcb, FORKNOEXEC);

    /* 设置ASID */
    get_process_mm(pcb)->asid = get_process_pid(pcb);

    /* 切换空间 */
    mm_switch_space_to(get_process_mm(pcb));

    ret = vfs_stat(pcb->cmd_path, &st, 0);
    if (ret == 0)
    {
        if (st.st_mode & S_ISUID)
        {
            pcb->suid = pcb->euid;
            pcb->euid = st.st_uid;
        }
        if (st.st_mode & S_ISGID)
        {
            pcb->sgid = pcb->egid;
            pcb->egid = st.st_gid;
        }
    }
    ret = file_open(&f, pcb->cmd_path, O_RDONLY);

    if (ret < 0)
    {
        KLOG_E("%s : %s", pcb->cmd_path, strerror(-ret));
        return ret;
    }

    /*
     * 先完成新映像装载，再按 exec 语义重置信号状态。
     * 这样 load_elf 失败时，当前进程仍保留原来的 signal 上下文，不会被提前破坏。
     */
    ret = load_elf(&f, pcb, (const char **)kargv, (const char **)kenvp);
    file_close(&f);

    if (ret < 0)
    {
        /* load_elf 失败时，本次 exec 申请的字符串参数需要立即回收。 */
        strings_free(kargv);
        strings_free(kenvp);
        return ret;
    }

    /* 仅在新映像已经建立成功后，才提交 exec 对信号处理状态的重置。 */
    ret = signal_reset(pcb);
    if (ret < 0)
    {
        /* signal_reset 失败时，同样回收本次 exec 临时构造出来的参数区。 */
        strings_free(kargv);
        strings_free(kenvp);
        return ret;
    }

    /* 移除旧的 envp并更新 */
    process_obj_destroy(pcb->envp);
    pcb->envp = process_obj_create(pcb, (void *)kenvp, (void (*)(void *))strings_free);

    strings_free(kargv);

    files_close_on_exec(get_process_vfs_list(pcb));

    if (pcb->vfork_done)
    {
        vfork_exec_wake(pcb);
    }

    trace_exec(pcb->taskControlId->objCore.objName);
    arch_run2user();
    return 0;
}
