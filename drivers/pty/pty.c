/****************************************************************************
 * drivers/pty/pty.c
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

/**
 * @file    drivers/pty/pty.c
 * @author  zyh
 * @brief
 * @version 3.0.0
 * @date    2024-07-30
 *
 * @note    基于 Apache NuttX 对应 PTY 驱动实现移植到当前 RTOS。
 * @note    已针对 VFS、pipe、TTY、进程组和平台环境进行修改。
 *
 * 科东(广州)软件科技有限公司 版权所有
 * @copyright Copyright (C) 2023 Intewell Inc. All Rights Reserved.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <ctype.h>
#include <driver/class/chardev.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <fs/kpoll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ttydefaults.h>
#include <sys/types.h>
#include <termios.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <uapi/ascii.h>
#include <unistd.h>

#include "pty.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Maximum number of threads than can be waiting for POLL events */

#ifndef CONFIG_DEV_PTY_NPOLLWAITERS
#define CONFIG_DEV_PTY_NPOLLWAITERS 2
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct pty_poll_s
{
    void *src;
    void *sink;
};

/* This device structure describes on memory of the PTY device pair */

struct pty_devpair_s;
struct pty_dev_s
{
    struct pty_devpair_s *pd_devpair;
    struct file pd_src;  /* Provides data to read() method (pipe output) */
    struct file pd_sink; /* Accepts data from write() method (pipe input) */
    bool pd_master;      /* True: this is the master */
    uint8_t pd_escape;   /* Number of the character to be escaped */
    tcflag_t pd_iflag;   /* Terminal input modes */
    tcflag_t pd_lflag;   /* Terminal local modes */
    tcflag_t pd_oflag;   /* Terminal output modes */
    struct winsize winsize;
    pid_t gpid;
    pid_t pid;
    struct pty_poll_s pd_poll[CONFIG_DEV_PTY_NPOLLWAITERS];
};

/* This structure describes the pipe pair */

struct pty_devpair_s
{
    struct pty_dev_s pp_master; /* Maseter device */
    struct pty_dev_s pp_slave;  /* Slave device */

    bool pp_susv1;       /* SUSv1 or BSD style */
    bool pp_locked;      /* Slave is locked */
    bool pp_unlinked;    /* File has been unlinked */
    uint8_t pp_minor;    /* Minor device number */
    uint16_t pp_nopen;   /* Open file count */
    SEMA_ID pp_slavesem; /* Slave lock semaphore */
    MUTEX_ID pp_lock;    /* Mutual exclusion */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void pty_destroy(struct pty_devpair_s *devpair);
static int pty_pipe(struct pty_devpair_s *devpair);
static int pty_open(struct file *filep);
static int pty_close(struct file *filep);
static ssize_t pty_read(struct file *filep, char *buffer, size_t buflen);
static ssize_t pty_write(struct file *filep, const char *buffer, size_t buflen);
static int pty_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static int pty_poll(struct file *filep, struct kpollfd *fds, bool setup);
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int pty_unlink(struct inode *inode);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_pty_fops = {
    .open = pty_open,   /* open */
    .close = pty_close, /* close */
    .read = pty_read,   /* read */
    .write = pty_write, /* write */
    .ioctl = pty_ioctl, /* ioctl */
    .poll = pty_poll,   /* poll */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
    .unlink = pty_unlink, /* unlink */
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pty_destroy
 ****************************************************************************/

static void pty_destroy(struct pty_devpair_s *devpair)
{
    char devname[16];

    if (devpair->pp_susv1)
    {
        /* Free this minor number so that it can be reused */

        ptmx_minor_free(devpair->pp_minor);

        /* Un-register the slave device */

        snprintf(devname, 16, "/dev/pts/%d", devpair->pp_minor);
    }
    else
    {
        /* Un-register the master device (/dev/ptyN may have already been
         * unlinked).
         */

        snprintf(devname, 16, "/dev/pty%d", (int)devpair->pp_minor);
        unregister_driver(devname);

        /* Un-register the slave device */

        snprintf(devname, 16, "/dev/ttyp%d", devpair->pp_minor);
    }

    unregister_driver(devname);

    /* And free the device structure */

    TTOS_DeleteMutex(devpair->pp_lock);
    TTOS_DeleteSema(devpair->pp_slavesem);
    free(devpair);
}

/****************************************************************************
 * Name: pty_pipe
 ****************************************************************************/

static int pty_pipe(struct pty_devpair_s *devpair)
{
    struct file *pipe_a[2];
    struct file *pipe_b[2];
    int ret;

    /* Create two pipes:
     *
     *   pipe_a:  Master source, slave sink (TX, slave-to-master)
     *   pipe_b:  Master sink, slave source (RX, master-to-slave)
     */

    pipe_a[0] = &devpair->pp_master.pd_src;
    pipe_a[1] = &devpair->pp_slave.pd_sink;

    ret = file_pipe(pipe_a, CONFIG_PSEUDOTERM_TXBUFSIZE, O_CLOEXEC);
    if (ret < 0)
    {
        return ret;
    }

    pipe_b[0] = &devpair->pp_slave.pd_src;
    pipe_b[1] = &devpair->pp_master.pd_sink;

    ret = file_pipe(pipe_b, CONFIG_PSEUDOTERM_RXBUFSIZE, O_CLOEXEC);
    if (ret < 0)
    {
        file_close(pipe_a[0]);
        file_close(pipe_a[1]);
    }

    return ret;
}

/****************************************************************************
 * Name: pty_open
 ****************************************************************************/

static int pty_open(struct file *filep)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    struct pty_devpair_s *devpair;
    int ret = 0;

    inode = filep->f_inode;
    dev = inode->i_private;
    assert(dev != NULL && dev->pd_devpair != NULL);
    devpair = dev->pd_devpair;

    /* Get exclusive access to the device structure */

    ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* Wait if this is an attempt to open the slave device and the slave
     * device is locked.
     */

    if (!dev->pd_master)
    {
        /* Slave... Check if the slave driver is locked. */

        while (devpair->pp_locked)
        {
            /* Release the exclusive access before wait */

            TTOS_ReleaseMutex(devpair->pp_lock);

            /* Wait until unlocked.
             * We will also most certainly suspend here.
             */

            ret = TTOS_ObtainSema(devpair->pp_slavesem, TTOS_WAIT_FOREVER);
            if (ret != TTOS_OK)
            {
                return -ttos_ret_to_errno(ret);
            }

            /* Restore the semaphore count */

            TTOS_ReleaseSema(devpair->pp_slavesem);

            /* Get exclusive access to the device structure.  This might also
             * cause suspension.
             */

            ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
            if (ret != TTOS_OK)
            {
                return -ttos_ret_to_errno(ret);
            }
        }
    }

    /* First open? */

    if (devpair->pp_nopen == 0)
    {
        /* Yes, create the internal pipe */

        ret = pty_pipe(devpair);
    }

    /* Increment the count of open references on the driver */

    if (ret >= 0)
    {
        devpair->pp_nopen++;
        assert(devpair->pp_nopen > 0);
    }

    TTOS_ReleaseMutex(devpair->pp_lock);

    if (!(filep->f_oflags & O_NOCTTY))
    {
        pcb_t pcb = ttosProcessSelf();
        if (pcb == NULL)
        {
            return ret;
        }
        /* 当前进程没有控制终端，且是会话首进程 */
        if (get_process_ctty(pcb) == NULL && pcb->sid == get_process_pid(pcb))
        {
            tty_set_ctty(inode, pcb);
        }
    }

    return ret;
}

/****************************************************************************
 * Name: pty_close
 ****************************************************************************/

static int pty_close(struct file *filep)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    struct pty_devpair_s *devpair;
    int ret;

    inode = filep->f_inode;
    dev = inode->i_private;
    assert(dev != NULL && dev->pd_devpair != NULL);
    devpair = dev->pd_devpair;

    /* Get exclusive access */

    ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* Check if the decremented inode reference count would go to zero */

    if (inode->i_crefs == 1)
    {
        /* Did the (single) master just close its reference? */

        if (dev->pd_master && devpair->pp_susv1)
        {
            /* Yes, then we are essentially unlinked and when all of the
             * slaves close there references, then the PTY should be
             * destroyed.
             */

            devpair->pp_unlinked = true;
        }

        /* Close the contained file structures */

        file_close(&dev->pd_src);
        file_close(&dev->pd_sink);
    }

    /* Is this the last open reference?  If so, was the driver previously
     * unlinked?
     */

    assert(devpair->pp_nopen > 0);
    if (devpair->pp_nopen <= 1 && devpair->pp_unlinked)
    {
        /* Yes.. Free the device pair now (without freeing the semaphore) */
        TTOS_ReleaseMutex(devpair->pp_lock);
        pty_destroy(devpair);
        return 0;
    }
    else
    {
        /* Otherwise just decrement the open count */

        devpair->pp_nopen--;
    }

    TTOS_ReleaseMutex(devpair->pp_lock);
    return 0;
}

/****************************************************************************
 * Name: pty_read
 ****************************************************************************/

static ssize_t pty_read(struct file *filep, char *buffer, size_t len)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    ssize_t ntotal;
    ssize_t i;
    ssize_t j;
    char ch;

    inode = filep->f_inode;
    dev = inode->i_private;
    assert(dev != NULL);

    /* Do input processing if any is enabled
     *
     * Specifically not handled:
     *
     *   All of the local modes; echo, line editing, etc.
     *   Anything to do with break or parity errors.
     *   ISTRIP     - We should be 8-bit clean.
     *   IUCLC      - Not Posix
     *   IXON/OXOFF - No xon/xoff flow control.
     */

    if (dev->pd_iflag & (INLCR | IGNCR | ICRNL))
    {
        while ((ntotal = file_read(&dev->pd_src, buffer, len)) > 0)
        {
            for (i = j = 0; i < ntotal; i++)
            {
                /* Perform input processing */

                ch = buffer[i];

                /* \n -> \r or \r -> \n translation? */

                if (ch == '\n' && (dev->pd_iflag & INLCR) != 0)
                {
                    ch = '\r';
                }
                else if (ch == '\r' && (dev->pd_iflag & ICRNL) != 0)
                {
                    ch = '\n';
                }

                /* Discarding \r ?  Print character if (1) character is not \r
                 * or if (2) we were not asked to ignore \r.
                 */

                if (ch != '\r' || (dev->pd_iflag & IGNCR) == 0)
                {
                    buffer[j++] = ch;
                }
            }

            /* Is the buffer not empty after process? */

            if (j != 0)
            {
                /* Yes, we are done. */

                ntotal = j;
                break;
            }
        }
    }
    else
    {
        /* NOTE: the source pipe will block if no data is available in
         * the pipe.   Otherwise, it will return data from the pipe.  If
         * there are fewer than 'len' bytes in the, it will return with
         * ntotal < len.
         *
         * REVISIT: Should not block if the oflags include O_NONBLOCK.
         * How would we ripple the O_NONBLOCK characteristic to the
         * contained source pipe? file_fcntl()?  Or FIONREAD?  See the
         * TODO comment at the top of this file.
         */

        ntotal = file_read(&dev->pd_src, buffer, len);
    }

    if ((dev->pd_lflag & ECHO) && (ntotal > 0))
    {
        size_t n = 0;

        for (i = j = 0; i < ntotal; i++)
        {
            ch = buffer[i];

            /* Check for the beginning of a VT100 escape sequence, 3 byte */

            if (ch == ASCII_ESC)
            {
                /* Mark that we should skip 2 more bytes */

                dev->pd_escape = 2;
                continue;
            }
            else if (dev->pd_escape == 2 && ch != ASCII_LBRACKET)
            {
                /* It's not an <esc>[x 3 byte sequence, show it */

                dev->pd_escape = 0;
            }
            else if (dev->pd_escape > 0)
            {
                /* Skipping character count down */

                if (--dev->pd_escape > 0)
                {
                    continue;
                }
            }

            /* Echo if the character in batch */

            if (ch == '\n' || (n != 0 && j + n != i))
            {
                if (n != 0)
                {
                    pty_write(filep, buffer + j, n);
                    n = 0;
                }

                if (ch == '\n')
                {
                    pty_write(filep, "\r\n", 2);
                    continue;
                }
            }

            /* Record the character can be echo */

            if (!iscntrl(ch & 0xff) && n++ == 0)
            {
                j = i;
            }
        }

        if (n != 0)
        {
            pty_write(filep, buffer + j, n);
        }
    }

    return ntotal;
}

/****************************************************************************
 * Name: pty_write
 ****************************************************************************/
static ssize_t pty_write(struct file *filep, const char *buffer, size_t len)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    ssize_t ntotal;
    ssize_t nwritten;
    size_t i;
    char ch;

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    int signo = 0;
#endif

    inode = filep->f_inode;
    dev = inode->i_private;
    assert(dev != NULL);

    // KLOG_D("%s write %d: [%s]\n", dev->pd_master ? "master" : "slave", len,
    // buffer);

    /* Do output post-processing */

    if ((dev->pd_oflag & OPOST) != 0)
    {
        /* We will transfer one byte at a time, making the appropriae
         * translations.  Specifically not handled:
         *
         *   OXTABS - primarily a full-screen terminal optimisation
         *   ONOEOT - Unix interoperability hack
         *   OLCUC  - Not specified by POSIX
         *   ONOCR  - low-speed interactive optimisation
         */

        ntotal = 0;
        for (i = 0; i < len; i++)
        {
            ch = *buffer++;

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
            signo = tty_check_special(dev->pd_devpair->pp_slave.pd_lflag, &ch, 1);
#endif

            /* Mapping CR to NL? */

            if (ch == '\r' && (dev->pd_oflag & OCRNL) != 0)
            {
                ch = '\n';
            }
            /* Are we interested in newline processing? */

            if ((ch == '\n') && (dev->pd_oflag & (ONLCR | ONLRET)) != 0)
            {
                char cr = '\r';

                /* Transfer the carriage return.  This will block if the
                 * sink pipe is full.
                 *
                 * REVISIT: Should not block if the oflags include O_NONBLOCK.
                 * How would we ripple the O_NONBLOCK characteristic to the
                 * contained sink pipe?  file_fcntl()?  Or FIONSPACE?  See the
                 * TODO comment at the top of this file.
                 *
                 * NOTE: The newline is not included in total number of bytes
                 * written.  Otherwise, we would return more than the
                 * requested number of bytes.
                 */

                nwritten = file_write(&dev->pd_sink, &cr, 1);
                if (nwritten < 0)
                {
                    ntotal = nwritten;
                    break;
                }
            }

            /* Transfer the (possibly translated) character..  This will block
             * if the sink pipe is full
             *
             * REVISIT: Should not block if the oflags include O_NONBLOCK.
             * How would we ripple the O_NONBLOCK characteristic to the
             * contained sink pipe?  file_fcntl()?  Or FIONSPACe?  See the
             * TODO comment at the top of this file.
             */

            nwritten = file_write(&dev->pd_sink, &ch, 1);
            if (nwritten < 0)
            {
                ntotal = nwritten;
                break;
            }

            /* Update the count of bytes transferred */

            ntotal++;
        }
    }
    else
    {
        /* Write the 'len' bytes to the sink pipe.  This will block until all
         * 'len' bytes have been written to the pipe.
         *
         * REVISIT: Should not block if the oflags include O_NONBLOCK.
         * How would we ripple the O_NONBLOCK characteristic to the
         * contained sink pipe?  file_fcntl()?  Or FIONSPACE?  See the
         * TODO comment at the top of this file.
         */

        ntotal = file_write(&dev->pd_sink, buffer, len);
    }

    if (signo != 0 && dev->pd_master)
    {
        if (dev->pd_devpair->pp_slave.gpid != INVALID_PROCESS_ID)
        {
            kernel_signal_send(dev->pd_devpair->pp_slave.gpid, TO_PGROUP, signo, SI_KERNEL, 0);
        }
        // char_reset_sem (dev);
    }

    return ntotal;
}

/****************************************************************************
 * Name: pty_ioctl
 *
 * Description:
 *   The standard ioctl method.  This is where ALL of the PWM work is done.
 *
 ****************************************************************************/

static int pty_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    struct pty_devpair_s *devpair;
    int ret;

    inode = filep->f_inode;
    dev = inode->i_private;
    assert(dev != NULL && dev->pd_devpair != NULL);
    devpair = dev->pd_devpair;

    /* Get exclusive access */

    ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* Handle IOCTL commands */

    switch (cmd)
    {
    case TIOCSCTTY:
    {
        ret = tty_set_ctty(inode, ttosProcessSelf());
        if (ret == 0)
        {
            /* 官方文档中说此操作不应该设置前台进程组，但是如果不设置 dropbear 并不会主动设置 */
            dev->gpid = ttosProcessSelf()->sid;
        }
    }
    break;

    case TIOCNOTTY:
    {
        pcb_t pcb = ttosProcessSelf();
        /* 只有当前进程是会话首进程且控制终端是当前终端时 才能释放控制终端 */
        if (get_process_ctty(pcb) == inode && pcb->sid == get_process_pid(pcb))
        {
            tty_clear_ctty(pcb);
        }
        else
        {
            return -EPERM;
        }
        ret = 0;
    }
    break;
#define FIOC_FILEPATH 0x0303
        extern int inode_getpath(struct inode * node, char *path, size_t len);
    case FIOC_FILEPATH:
    {
        char *ptr = (char *)((uintptr_t)arg);
        ret = inode_getpath(filep->f_inode, ptr, PATH_MAX);

        if (ret < 0)
        {
            return ret;
        }
    }
    break;
    case TIOCGPGRP:
    {
        int *pgrp = (int *)arg;

        pcb_t pcb = ttosProcessSelf();
        if (get_process_ctty(pcb) == inode)
        {
            if (dev->gpid < 0)
            {
                /* 如果没有前台进程组, 且当前进程的控制终端是当前tty，则tcgetpgrp() 应返回一个大于 1
                 * 的值，该值不匹配任何现有的进程组 ID */
                *pgrp =
                    65536; // 65536 is a common value used to indicate no foreground process group
            }
            else
            {
                *pgrp = dev->gpid;
            }
            ret = 0;
        }
        else
        {
            /* 如果当前进程的控制终端不是当前tty，则tcgetpgrp() 应返回 -1 */
            *pgrp = -1;
            ret = -ENOTTY;
        }
    }
    break;
    case TIOCSPGRP:
    {
        pid_t gpid = *(int *)arg;
        pcb_t pcb = pcb_get_by_pid_nt(gpid);
        if (pcb == NULL)
        {
            ret = -EINVAL;
            break;
        }
        /* 当前进程的控制终端不是当前终端 */
        if (get_process_ctty(pcb) != inode)
        {
            ret = -EPERM;
            break;
        }
        dev->gpid = gpid;
        ret = 0;
    }
    break;
        /* 获取 窗口大小 */
    case TIOCGWINSZ:
    {
        struct winsize *pw = (struct winsize *)((uintptr_t)arg);

        /* Get row/col from the private data */
        *pw = dev->winsize;
        ret = 0;
    }
    break;
    case TIOCSWINSZ:
    {
        struct winsize *pw = (struct winsize *)((uintptr_t)arg);

        /* Set row/col from the private data */

        if (memcmp(pw, &dev->winsize, sizeof(dev->winsize)))
        {
            dev->winsize = *pw;
            if (dev->gpid != INVALID_PROCESS_ID)
            {
                kernel_signal_send(dev->gpid, TO_PGROUP, SIGWINCH, SI_KERNEL, 0);
            }
        }
        ret = 0;
    }
    break;
        /* PTY IOCTL commands would be handled here */

    case TIOCGPTN: /* Get Pty Number (of pty-mux device): int* */
    {
        int *ptyno = (int *)((uintptr_t)arg);
        if (ptyno == NULL)
        {
            ret = -EINVAL;
        }
        else
        {
            *ptyno = devpair->pp_minor;
            ret = 0;
        }
    }
    break;

    case TIOCSPTLCK: /* Lock/unlock Pty: int */
    {
        int *lock = (int *)((uintptr_t)arg);
        if (*lock == 0)
        {
            if (devpair->pp_locked)
            {
                /* Release any waiting threads */

                ret = TTOS_ReleaseSema(devpair->pp_slavesem);
                if (ret == TTOS_OK)
                {
                    devpair->pp_locked = false;
                }
            }
        }
        else if (!devpair->pp_locked)
        {
            /* Locking */

            ret = TTOS_ObtainSema(devpair->pp_slavesem, TTOS_WAIT_FOREVER);
            if (ret == TTOS_OK)
            {
                devpair->pp_locked = true;
            }
        }
    }
    break;

    case TIOCGPTLCK: /* Get Pty lock state: int* */
    {
        int *ptr = (int *)((uintptr_t)arg);
        if (ptr == NULL)
        {
            ret = -EINVAL;
        }
        else
        {
            *ptr = devpair->pp_locked;
            ret = 0;
        }
    }
    break;

    case TCGETS:
    {
        struct termios *termiosp = (struct termios *)arg;

        if (!termiosp)
        {
            ret = -EINVAL;
            break;
        }

        /* And update with flags from this layer */

        termiosp->c_iflag = dev->pd_devpair->pp_slave.pd_iflag;
        termiosp->c_oflag = dev->pd_devpair->pp_slave.pd_oflag;
        termiosp->c_lflag = dev->pd_devpair->pp_slave.pd_lflag;
        ret = 0;
    }
    break;

    case TCSETS:
    {
        struct termios *termiosp = (struct termios *)arg;

        if (!termiosp)
        {
            ret = -EINVAL;
            break;
        }

        /* Update the flags we keep at this layer */

        dev->pd_devpair->pp_slave.pd_iflag = termiosp->c_iflag;
        dev->pd_devpair->pp_slave.pd_oflag = termiosp->c_oflag;
        dev->pd_devpair->pp_slave.pd_lflag = termiosp->c_lflag;
        ret = 0;
    }
    break;

        /* Get the number of bytes that are immediately available for reading
         * from the source pipe.
         */
    case FIONREAD:
    {
        ret = file_ioctl(&dev->pd_src, cmd, arg);
    }
    break;

    /* Get the number of bytes waiting in the sink pipe (FIONWRITE) or the
     * number of unused bytes in the sink pipe (FIONSPACE).
     */
#if 0
    case FIONWRITE:
    case FIONSPACE:
        {
            ret = file_ioctl (&dev->pd_sink, cmd, arg);
        }
        break;
#endif
    case FIONBIO:
    {
        ret = file_ioctl(&dev->pd_src, cmd, arg);
        if (ret >= 0)
        {
            ret = file_ioctl(&dev->pd_sink, cmd, arg);
        }
    }
    break;

        /* Any unrecognized IOCTL commands will be passed to the contained
         * pipe driver.
         *
         * REVISIT:  We know for a fact that the pipe driver only supports
         * FIONREAD, FIONWRITE, FIONSPACE and PIPEIOC_POLICY.  The first two
         * are handled above and PIPEIOC_POLICY should not be managed by
         * applications -- it can break the PTY!
         */

    default:
    {
        ret = file_ioctl(&dev->pd_src, cmd, arg);
        if (ret >= 0 || ret == -ENOTTY)
        {
            ret = file_ioctl(&dev->pd_sink, cmd, arg);
        }
    }
    break;
    }

    TTOS_ReleaseMutex(devpair->pp_lock);
    return ret;
}

/****************************************************************************
 * Name: pty_poll
 ****************************************************************************/

static int pty_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    struct inode *inode;
    struct pty_dev_s *dev;
    struct pty_devpair_s *devpair;
    struct pty_poll_s *pollp = NULL;
    int ret;
    int i;

    inode = filep->f_inode;
    dev = inode->i_private;
    devpair = dev->pd_devpair;

    ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    if (setup)
    {
        for (i = 0; i < CONFIG_DEV_PTY_NPOLLWAITERS; i++)
        {
            if (dev->pd_poll[i].src == NULL && dev->pd_poll[i].sink == NULL)
            {
                pollp = &dev->pd_poll[i];
                break;
            }
        }

        if (i >= CONFIG_DEV_PTY_NPOLLWAITERS)
        {
            ret = -EBUSY;
            goto errout;
        }
    }
    else
    {
        pollp = (struct pty_poll_s *)fds->priv;
    }

    /* POLLIN: Data may be read without blocking. */

    if ((fds->pollfd.events & POLLIN) != 0)
    {
        fds->priv = pollp->src;
        ret = file_poll(&dev->pd_src, fds, setup);
        if (ret < 0)
        {
            goto errout;
        }

        pollp->src = fds->priv;
    }

    /* POLLOUT: Normal data may be written without blocking. */

    if ((fds->pollfd.events & POLLOUT) != 0)
    {
        fds->priv = pollp->sink;
        ret = file_poll(&dev->pd_sink, fds, setup);
        if (ret < 0)
        {
            if (pollp->src)
            {
                fds->priv = pollp->src;
                file_poll(&dev->pd_src, fds, false);
                pollp->src = NULL;
            }

            goto errout;
        }

        pollp->sink = fds->priv;
    }

    if (setup)
    {
        fds->priv = pollp;
    }

errout:
    TTOS_ReleaseMutex(devpair->pp_lock);
    return ret;
}

/****************************************************************************
 * Name: pty_unlink
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int pty_unlink(struct inode *inode)
{
    struct pty_dev_s *dev;
    struct pty_devpair_s *devpair;
    int ret;

    assert(inode->i_private != NULL);
    dev = inode->i_private;
    devpair = dev->pd_devpair;
    assert(dev->pd_devpair != NULL);

    /* Get exclusive access */

    ret = TTOS_ObtainMutex(devpair->pp_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* Indicate that the driver has been unlinked */

    devpair->pp_unlinked = true;

    /* If there are no further open references to the driver, then commit
     * Hara-Kiri now.
     */

    if (devpair->pp_nopen == 0)
    {
        TTOS_ReleaseMutex(devpair->pp_lock);
        pty_destroy(devpair);
        return 0;
    }

    TTOS_ReleaseMutex(devpair->pp_lock);
    return 0;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pty_register2
 *
 * Description:
 *   Create and register PTY master and slave devices.  The slave side of
 *   the interface is always locked initially.  The master must call
 *   unlockpt() before the slave device can be opened.
 *
 * Input Parameters:
 *   minor - The number that qualifies the naming of the created devices.
 *   susv1 - select SUSv1 or BSD behaviour
 *
 * Returned Value:
 *   0 is returned on success; otherwise, the negative error code return
 *   appropriately.
 *
 ****************************************************************************/

int pty_register2(int minor, bool susv1)
{
    struct pty_devpair_s *devpair;
    char devname[16];
    int ret;

    /* Allocate a device instance */

    devpair = calloc(1, sizeof(struct pty_devpair_s));
    if (devpair == NULL)
    {
        return -ENOMEM;
    }

    /* Initialize semaphores & mutex */

    TTOS_CreateMutex(1, 0, &devpair->pp_lock);
    TTOS_CreateSemaEx(0, &devpair->pp_slavesem);

    /* Map CR -> NL from terminal input (master)
     * For some usage like adb shell:
     *   adb shell write \r -> nsh read \n and echo input
     *   nsh write \n -> adb shell read \r\n
     */

    devpair->pp_susv1 = susv1;
    devpair->pp_minor = minor;
    devpair->pp_locked = true;
    devpair->pp_master.pd_devpair = devpair;
    devpair->pp_master.pd_master = true;
    devpair->pp_master.pd_oflag = OPOST;
    devpair->pp_master.pd_lflag = 0;
    devpair->pp_master.pd_iflag = 0;

    devpair->pp_slave.pd_devpair = devpair;
    devpair->pp_slave.pd_oflag = TTYDEF_OFLAG;
    devpair->pp_slave.pd_lflag = TTYDEF_LFLAG;
    devpair->pp_slave.pd_iflag = TTYDEF_IFLAG;
    devpair->pp_slave.pid = INVALID_PROCESS_ID;
    devpair->pp_slave.gpid = INVALID_PROCESS_ID;

    /* Register the master device
     *
     * BSD style (deprecated): /dev/ptyN
     * SUSv1 style: Master: /dev/ptmx (multiplexor, see ptmx.c)
     *
     * Where N is the minor number
     */

    snprintf(devname, 16, "/dev/pty%d", minor);

    ret = register_driver(devname, &g_pty_fops, 0666, &devpair->pp_master);
    if (ret < 0)
    {
        goto errout_with_devpair;
    }

    /* Register the slave device
     *
     * BSD style (deprecated): /dev/ttypN
     * SUSv1 style: /dev/pts/N
     *
     * Where N is the minor number
     */

    if (susv1)
    {
        snprintf(devname, 16, "/dev/pts/%d", minor);
    }
    else
    {
        snprintf(devname, 16, "/dev/ttyp%d", minor);
    }

    ret = register_driver(devname, &g_pty_fops, 0666, &devpair->pp_slave);
    if (ret < 0)
    {
        goto errout_with_master;
    }

    return 0;

errout_with_master:
    snprintf(devname, 16, "/dev/pty%d", minor);
    unregister_driver(devname);

errout_with_devpair:
    TTOS_DeleteMutex(devpair->pp_lock);
    TTOS_DeleteSema(devpair->pp_slavesem);
    free(devpair);
    return ret;
}

/****************************************************************************
 * Name: pty_register
 *
 * Description:
 *   Create and register PTY master and slave devices.  The master device
 *   will be registered at /dev/ptyN and slave at /dev/ttypN where N is
 *   the provided minor number.
 *
 *   The slave side of the interface is always locked initially.  The
 *   master must call unlockpt() before the slave device can be opened.
 *
 * Input Parameters:
 *   minor - The number that qualifies the naming of the created devices.
 *
 * Returned Value:
 *   Zero (0) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int pty_register(int minor)
{
    return pty_register2(minor, false);
}
