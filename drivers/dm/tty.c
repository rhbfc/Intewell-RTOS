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

#include <fs/fs.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <tty.h>
#include <uapi/ascii.h>

/**
 * @brief 清除控制终端
 */
int tty_clear_ctty(pcb_t pcb)
{
    tty_t tty;
    tty = get_process_ctty(pcb);
    /* 当前进程没有控制终端直接返回 */
    if (tty == NULL)
    {
        return 0;
    }
    get_process_signal(pcb)->ctty = NULL;
    return 0;
}

/**
 * @brief 设置控制终端
 */
int tty_set_ctty(tty_t tty, pcb_t pcb)
{
    /* 当前进程已经设置为此控制终端 */
    if (get_process_ctty(pcb) == tty)
    {
        return 0;
    }

    /* 当前进程已有控制终端 */
    if (get_process_ctty(pcb) != NULL)
    {
        return -EPERM;
    }

    /* 当前进程不是 session leader */
    if (pcb->sid != get_process_pid(pcb))
    {
        return -EPERM;
    }
    get_process_signal(pcb)->ctty = (tty_t)tty;
    return 0;
}

int tty_check_special(tcflag_t tc_lflag, const char *buf, size_t size)
{
    size_t i;
    if ((tc_lflag & ISIG) == 0)
    {
        return 0;
    }

    for (i = 0; i < size; i++)
    {
        switch (buf[i])
        {
        /* ^C */
        case ASCII_ETX:
            return SIGINT;
        /* ^\ */
        case ASCII_FS:
            return SIGQUIT;
        /* ^Z */
        case ASCII_SUB:
            return SIGTSTP;
        default:
            break;
        }
    }

    return 0;
}

static int tty_open(struct file *filep)
{
    pcb_t pcb = ttosProcessSelf();
    int ret;
    if (pcb == NULL)
    {
        return -EINVAL;
    }

    if (get_process_ctty(pcb) == NULL)
    {
        return -ENXIO;
    }
    filep->f_inode = get_process_ctty(pcb);

    if (INODE_IS_DRIVER(filep->f_inode) || INODE_IS_PIPE(filep->f_inode))
    {
        if (filep->f_inode->u.i_ops->open != NULL)
        {
            ret = filep->f_inode->u.i_ops->open(filep);
        }
    }
    else
    {
        ret = -ENXIO;
    }
    return 0;
}
static const struct file_operations g_tty_fops = {
    .open = tty_open, /* open */
};

static int tty_register(void)
{
    return register_driver("/dev/tty", &g_tty_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER(tty_register, "tty_register");