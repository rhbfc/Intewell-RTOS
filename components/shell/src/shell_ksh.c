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

#include <driver/device.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <inttypes.h>
#include <shell.h>
#include <ttos.h>
#include <ttosBase.h>

static int fd[2];
static struct file *fpipe;
static struct device ksh;

static ssize_t ksh_write(struct file *filep, const char *buffer, size_t buflen)
{
    return file_write(fpipe, buffer, buflen);
}

static short ksh_read(char *data, unsigned short len)
{
    return read(fd[0], data, len);
}

static const struct file_operations ksh_fops = {
    .write = ksh_write,
};

void shell_ksh_init(Shell *shell)
{
    vfs_pipe2(fd, 0);

    fs_getfilep(fd[1], &fpipe);

    shell->read = ksh_read;
    shell->ksh = true;

    initialize_device(&ksh);

    device_bind_path(&ksh, "/dev/ksh", &ksh_fops, 0666);
}

static int list_sem(int argc, const char *argv[])
{
    int num, i;
    SEMA_ID idList[128];

    num = ttosGetIdList((T_TTOS_ObjectCoreID *)idList, 128, TTOS_OBJECT_SEMA);
    if (sizeof(void *) == 8)
        printk("         ID         type      value   holder\n");
    else
        printk("    ID       type      value   holder\n");

    for (i = 0; i < num; i++)
    {
        printk("0x%" PRIXPTR "  %-6s  %6d    %-32s\n", (uintptr_t)idList[i]->semaControlId,
               idList[i]->attributeSet & TTOS_COUNTING_SEMAPHORE ? "sem" : "mutex",
               idList[i]->semaControlValue,
               (idList[i]->semHolder != TTOS_NULL_OBJECT_ID)
                   ? (char *)idList[i]->semHolder->objCore.objName
                   : "-");
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 lssem, list_sem, sem status);

static int list_msq(int argc, const char *argv[])
{
    int num, i;
    MSGQ_ID idList[128];

    num = ttosGetIdList((T_TTOS_ObjectCoreID *)idList, 128, TTOS_OBJECT_MSGQ);
    if (sizeof(void *) == 8)
        printk("         ID         MaxSize       PendCount     MaxPcount\n");
    else
        printk("    ID      MaxSize       PendCount     MaxPcount\n");

    for (i = 0; i < num; i++)
    {
        printk("0x%" PRIXPTR "    0x%X           %d              %d\n", (uintptr_t)idList[i],
               idList[i]->maxMsgSize, idList[i]->pendingCount, idList[i]->maxPendingMsg);
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 lsmsq, list_msq, sem status);