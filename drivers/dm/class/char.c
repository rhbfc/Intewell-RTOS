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
#include <driver/class/chardev.h>
#include <driver/cpudev.h>
#include <driver/device.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <poll.h>
#include <process_signal.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/ttydefaults.h>
#include <sys/types.h>
#include <time.h>
#include <time/ktime.h>
#include <ttosInterTask.inl>
#include <uapi/ascii.h>
#include <unistd.h>

#include <ttosProcess.h>

#define KLOG_TAG "CHAR"
#include <klog.h>

/* Timing */

#define POLL_DELAY_USEC 1000

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Write support */

static int char_putxmitchar(struct device *dev, int ch, bool oktoblock);
static inline ssize_t char_irqwrite(struct device *dev, const char *buffer, size_t buflen);
static int char_tcdrain(struct device *dev, clock_t timeout);

static int char_tcsendbreak(struct device *dev, struct file *filep, unsigned int ms);

/* Character driver methods */

static int char_open(struct file *filep);
static int char_close(struct file *filep);
static ssize_t char_read(struct file *filep, char *buffer, size_t buflen);
static ssize_t char_write(struct file *filep, const char *buffer, size_t buflen);
static int char_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static int char_poll(struct file *filep, struct kpollfd *fds, bool setup);

extern bool CAD_is_signal_to_init(void);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_TTY_LAUNCH_ENTRY
/* Lanch program entry, this must be supplied by the application. */

int CONFIG_TTY_LAUNCH_ENTRYPOINT(int argc, char *argv[]);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_serialops = {.open = char_open,   /* open */
                                                   .close = char_close, /* close */
                                                   .read = char_read,   /* read */
                                                   .write = char_write, /* write */
                                                   .seek = NULL,        /* seek */
                                                   .ioctl = char_ioctl, /* ioctl */
                                                   .mmap = NULL,        /* mmap  */
                                                   .poll = char_poll,   /* poll todo */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
                                                   .unlink = NULL
#endif
};

#ifdef CONFIG_TTY_LAUNCH
static struct work_s g_serial_work;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: char_putxmitchar
 ****************************************************************************/

static int char_putxmitchar(struct device *dev, int ch, bool oktoblock)
{
    long flags;
    int ret;

    struct char_class_type *class = (struct char_class_type *)dev->class;

    /* Loop until we are able to add the character to the TX buffer. */

    for (;;)
    {
        /* Check if the TX buffer is full */

        if (!circbuf_is_full(&class->xmit.buf))
        {
            /* No.. not full.  Add the character to the TX buffer and return. */

            circbuf_write(&class->xmit.buf, &ch, 1);
            return 0;
        }

        /* The TX buffer is full.  Should be block, waiting for the hardware
         * to remove some data from the TX buffer?
         */

        else if (oktoblock)
        {
            /* The following steps must be atomic with respect to serial
             * interrupt handling.
             */

            ttos_int_lock(flags);

            /* Check again...  In certain race conditions an interrupt may
             * have occurred between the test at the top of the loop and
             * entering the critical section and the TX buffer may no longer
             * be full.
             *
             * NOTE: On certain devices, such as USB CDC/ACM, the entire TX
             * buffer may have been emptied in this race condition.  In that
             * case, the logic would hang below waiting for space in the TX
             * buffer without this test.
             */

            if (!circbuf_is_full(&class->xmit.buf))
            {
                ret = 0;
            }

#ifdef CONFIG_SERIAL_REMOVABLE
            /* Check if the removable device is no longer connected while we
             * have interrupts off.  We do not want the transition to occur
             * as a race condition before we begin the wait.
             */

            else if (class->disconnected)
            {
                ret = -ENOTCONN;
            }
#endif
            else
            {
                /* Wait for some characters to be sent from the buffer with
                 * the TX interrupt enabled.  When the TX interrupt is enabled,
                 * uart_xmitchars() should execute and remove some of the data
                 * from the TX buffer.
                 *
                 * NOTE that interrupts will be re-enabled while we wait for
                 * the semaphore.
                 */

#ifdef CONFIG_SERIAL_TXDMA
                char_dmatxavail(dev);
#endif
                char_enabletxint(dev);
                ret = TTOS_ObtainSema(class->xmitsem, TTOS_WAIT_FOREVER);
                char_disabletxint(dev);
            }

            ttos_int_unlock(flags);

#ifdef CONFIG_SERIAL_REMOVABLE
            /* Check if the removable device was disconnected while we were
             * waiting.
             */

            if (dev->disconnected)
            {
                return -ENOTCONN;
            }
#endif

            /* Check if we were awakened by signal. */

            if (ret != TTOS_OK)
            {
                /* A signal received while waiting for the xmit buffer to
                 * become non-full will abort the transfer.
                 */

                return -ttos_ret_to_errno(ret);
            }
        }

        /* The caller has request that we not block for data.  So return the
         * EAGAIN error to signal this situation.
         */

        else
        {
            return -EAGAIN;
        }
    }

    /* We won't get here.  Some compilers may complain that this code is
     * unreachable.
     */

    return 0;
}

/****************************************************************************
 * Name: char_putc
 ****************************************************************************/

static inline void char_putc(struct device *dev, int ch)
{
    while (!char_txready(dev))
    {
    }

    char_send(dev, ch);
}

/****************************************************************************
 * Name: char_irqwrite
 ****************************************************************************/

static inline ssize_t char_irqwrite(struct device *dev, const char *buffer, size_t buflen)
{
    ssize_t ret = buflen;
    struct char_class_type *class = (struct char_class_type *)dev->class;

    /* Force each character through the low level interface */

    for (; buflen; buflen--)
    {
        int ch = *buffer++;

        /* Do output post-processing */

        if ((class->tc_oflag & OPOST) != 0)
        {
            /* Mapping CR to NL? */

            if ((ch == '\r') && (class->tc_oflag & OCRNL) != 0)
            {
                ch = '\n';
            }

            /* Are we interested in newline processing? */

            if ((ch == '\n') && (class->tc_oflag & (ONLCR | ONLRET)) != 0)
            {
                char_putc(dev, '\r');
            }
        }

        /* Output the character, using the low-level direct UART interfaces */

        char_putc(dev, ch);
    }

    return ret;
}

/****************************************************************************
 * Name: char_tcdrain
 *
 * Description:
 *   Block further TX input.
 *   Wait until all data has been transferred from the TX buffer and
 *   until the hardware TX FIFOs are empty.
 *
 ****************************************************************************/

static int char_tcdrain(struct device *dev, clock_t timeout)
{
    int ret;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* tcdrain is a cancellation point */

    /* Get exclusive access to the to class->tmit.  We cannot permit new data to
     * be written while we are trying to flush the old data.
     *
     * A signal received while waiting for access to the xmit.head will abort
     * the operation with EINTR.
     */

    ret = TTOS_ObtainMutex(class->xmit.lock, TTOS_WAIT_FOREVER);
    if (ret == TTOS_OK)
    {
        long flags;
        T_UDWORD start, end;

        /* Trigger emission to flush the contents of the tx buffer */
        ttos_int_lock(flags);

#ifdef CONFIG_SERIAL_REMOVABLE
        /* Check if the removable device is no longer connected while we have
         * interrupts off.  We do not want the transition to occur as a race
         * condition before we begin the wait.
         */

        if (class->disconnected)
        {
            class->xmit.tail = class->xmit.head; /* Drop the buffered TX data */
            ret = -ENOTCONN;
        }
        else
#endif
        {
            /* Continue waiting while the TX buffer is not empty.
             *
             * NOTE: There is no timeout on the following loop.  In
             * situations were this loop could hang (with hardware flow
             * control, as an example),  the caller should call
             * tcflush() first to discard this buffered Tx data.
             */

            ret = 0;
            while (ret >= 0 && !circbuf_is_empty(&class->xmit.buf))
            {
                /* Wait for some characters to be sent from the buffer with
                 * the TX interrupt enabled.  When the TX interrupt is
                 * enabled, uart_xmitchars() should execute and remove some
                 * of the data from the TX buffer.  We may have to wait several
                 * times for the TX buffer to be entirely emptied.
                 *
                 * NOTE that interrupts will be re-enabled while we wait for
                 * the semaphore.
                 */

#ifdef CONFIG_SERIAL_TXDMA
                char_dmatxavail(dev);
#endif
                char_enabletxint(dev);
                ret = TTOS_ObtainSema(class->xmitsem, 1);
                char_disabletxint(dev);
            }
        }

        ttos_int_unlock(flags);

        /* The TX buffer is empty (or an error occurred).  But there still may
         * be data in the UART TX FIFO.  We get no asynchronous indication of
         * this event, so we have to do a busy wait poll.
         */

        if (ret < 0)
        {
            return -ttos_ret_to_errno(ret);
        }

        /* Set up for the timeout
         *
         * REVISIT:  This is a kludge.  The correct fix would be add an
         * interface to the lower half driver so that the tcflush() operation
         * all also cause the lower half driver to clear and reset the Tx FIFO.
         */

        TTOS_GetSystemTicks(&start);

        if (ret >= 0)
        {
            while (!char_txempty(dev))
            {
                clock_t elapsed;

                usleep(POLL_DELAY_USEC);

                /* Check for a timeout */
                TTOS_GetSystemTicks(&end);
                elapsed = end - start;
                if (elapsed >= timeout)
                {
                    TTOS_ReleaseMutex(class->xmit.lock);
                    return -ETIMEDOUT;
                }
            }
        }

        TTOS_ReleaseMutex(class->xmit.lock);
    }

    return ret;
}

/****************************************************************************
 * Name: char_tcsendbreak
 *
 * Description:
 *   Request a serial line Break by calling the lower half driver's
 *   BSD-compatible Break IOCTLs TIOCSBRK and TIOCCBRK, with a sleep of the
 *   specified duration between them.
 *
 * Input Parameters:
 *   dev      - Serial device.
 *   filep    - Required for issuing lower half driver IOCTL call.
 *   ms       - If non-zero, duration of the Break in milliseconds; if
 *              zero, duration is 400 milliseconds.
 *
 * Returned Value:
 *   0 on success or a negated errno value on failure.
 *
 ****************************************************************************/

static int char_tcsendbreak(struct device *dev, struct file *filep, unsigned int ms)
{
    int ret;

    struct char_class_type *class = (struct char_class_type *)dev->class;

    /* REVISIT: Do we need to perform the equivalent of tcdrain() before
     * beginning the Break to avoid corrupting the transmit data? If so, note
     * that just calling char_tcdrain() here would create a race condition,
     * since new transmit data could be written after char_tcdrain() returns
     * but before we re-acquire the dev->xmit.lock here. Therefore, we would
     * need to refactor the functional portion of char_tcdrain() to a separate
     * function and call it from both char_tcdrain() and char_tcsendbreak()
     * in critical section and with xmit lock already held.
     */

    if (class->ops->ioctl)
    {
        ret = TTOS_ObtainMutex(class->xmit.lock, TTOS_WAIT_FOREVER);
        if (ret == TTOS_OK)
        {
            /* Request lower half driver to start the Break */

            ret = class->ops->ioctl(filep, TIOCSBRK, 0);
            if (ret >= 0)
            {
                /* Wait 400 ms or the requested Break duration */

                usleep((ms == 0) ? 400000 : ms * 1000);

                /* Request lower half driver to end the Break */

                ret = class->ops->ioctl(filep, TIOCCBRK, 0);
            }
        }

        TTOS_ReleaseMutex(class->xmit.lock);
    }
    else
    {
        /* With no lower half IOCTL, we cannot request Break at all. */

        ret = -ENOTTY;
    }

    return ret;
}

/****************************************************************************
 * Name: char_open
 *
 * Description:
 *   This routine is called whenever a serial port is opened.
 *
 ****************************************************************************/

static int char_open(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    uint8_t tmp;
    int ret;

    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* If the port is the middle of closing, wait until the close is finished.
     * If a signal is received while we are waiting, then return EINTR.
     */

    ret = TTOS_ObtainMutex(class->closelock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        /* A signal received while waiting for the last close operation. */

        return -EAGAIN;
    }

#ifdef CONFIG_SERIAL_REMOVABLE
    /* If the removable device is no longer connected, refuse to open the
     * device.  We check this after obtaining the close semaphore because
     * we might have been waiting when the device was disconnected.
     */

    if (class->disconnected)
    {
        ret = -ENOTCONN;
        goto errout_with_lock;
    }
#endif

    /* Start up serial port */

    /* Increment the count of references to the device. */

    /* Check if this is the first time that the driver has been opened. */

    if (inode->i_crefs == 1)
    {
        long flags;

        ttos_int_lock(flags);

        class->aio.aio_enable = false;
        class->aio.pid = -1;
        /* If this is the console, then the UART has already been
         * initialized.
         */

        if (!class->isconsole)
        {
            /* Perform one time hardware initialization */

            ret = char_setup(dev);
            if (ret < 0)
            {
                ttos_int_unlock(flags);
                goto errout_with_lock;
            }
        }

        /* In any event, we do have to configure for interrupt driven mode of
         * operation.  Attach the hardware IRQ(s). Hmm.. should shutdown() the
         * the device in the rare case that char_attach() fails, tmp==1, and
         * this is not the console.
         */

        ret = char_attach(dev);
        if (ret < 0)
        {
            if (!class->isconsole)
            {
                char_shutdown(dev);
            }

            ttos_int_unlock(flags);
            goto errout_with_lock;
        }

#ifdef CONFIG_SERIAL_RXDMA
        /* Notify DMA that there is free space in the RX buffer */

        char_dmarxfree(dev);
#endif

        /* Enable the RX interrupt */

        char_enablerxint(dev);
        ttos_int_unlock(flags);
    }

    if (!(filep->f_oflags & O_NOCTTY))
    {
        pcb_t pcb = ttosProcessSelf();
        if (pcb == NULL)
        {
            goto errout_with_lock;
        }
        /* 当前进程没有控制终端，且是会话首进程 */
        if (get_process_ctty(pcb) == NULL && pcb->sid == get_process_pid(pcb))
        {
            tty_set_ctty(inode, pcb);
        }
    }

errout_with_lock:
    TTOS_ReleaseMutex(class->closelock);
    return ret;
}

/****************************************************************************
 * Name: char_close
 *
 * Description:
 *   This routine is called when the serial port gets closed.
 *   It waits for the last remaining data to be sent.
 *
 ****************************************************************************/

static int char_close(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    long flags;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* Get exclusive access to the close semaphore (to synchronize open/close
     * operations.
     * NOTE: that we do not let this wait be interrupted by a signal.
     * Technically, we should, but almost no one every checks the return value
     * from close() so we avoid a potential memory leak by ignoring signals in
     * this case.
     */

    TTOS_ObtainMutex(class->closelock, TTOS_WAIT_FOREVER);
    /* Use kobject reference counting instead */
    if (inode->i_crefs > 1)
    {
        TTOS_ReleaseMutex(class->closelock);
        return 0;
    }

    /* Stop accepting input */

    char_disablerxint(dev);

    /* Prevent blocking if the device is opened with O_NONBLOCK */

    if ((filep->f_oflags & O_NONBLOCK) == 0)
    {
        /* Now we wait for the transmit buffer(s) to clear */

        char_tcdrain(dev, 4 * TTOS_GetSysClkRate());
    }

    /* Free the IRQ and disable the UART */

    /* Disable interrupts */
    ttos_int_lock(flags);
    char_detach(dev);      /* Detach interrupts */
    if (!class->isconsole) /* Check for the serial console UART */
    {
        char_shutdown(dev); /* Disable the UART */
    }

    ttos_int_unlock(flags);

    /* Wake up read and poll functions */

    char_datareceived(dev);

    /* We need to re-initialize the semaphores if this is the last close
     * of the device, as the close might be caused by pthread_cancel() of
     * a thread currently blocking on any of them.
     */

    char_reset_sem(dev);
    TTOS_ReleaseMutex(class->closelock);
    return 0;
}

/****************************************************************************
 * Name: char_read
 ****************************************************************************/

static ssize_t char_read(struct file *filep, char *buffer, size_t buflen)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    struct char_buffer_s *rxbuf = &class->recv;
#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
    unsigned int nbuffered;
    unsigned int watermark;
#endif
    long flags;
    ssize_t recvd = 0;
    bool echoed = false;
    char ch;
    int ret;

    /* Only one user can access rxbuf->tail at a time */

    ret = TTOS_ObtainMutex(class->recv.lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        /* A signal received while waiting for access to the recv.tail will
         * abort the transfer.  After the transfer has started, we are
         * committed and signals will be ignored.
         */

        return -EAGAIN;
    }

    /* Loop while we still have data to copy to the receive buffer.
     * we add data to the head of the buffer; uart_xmitchars takes the
     * data from the end of the buffer.
     */

    while ((size_t)recvd < buflen)
    {
#ifdef CONFIG_SERIAL_REMOVABLE
        /* If the removable device is no longer connected, refuse to read any
         * further from the device.
         */

        if (dev->disconnected)
        {
            if (recvd == 0)
            {
                recvd = -ENOTCONN;
            }

            break;
        }
#endif

        /* Check if there is more data to return in the circular buffer.
         * NOTE: Rx interrupt handling logic may asynchronously increment
         * the head index but must not modify the tail index.  The tail
         * index is only modified in this function.  Therefore, no
         * special handshaking is required here.
         *
         * The head and tail pointers are 16-bit values.  The only time that
         * the following could be unsafe is if the CPU made two non-atomic
         * 8-bit accesses to obtain the 16-bit head index.
         */
        if (!circbuf_is_empty(&rxbuf->buf))
        {
            /* Take the next character from the tail of the buffer */
            circbuf_read(&rxbuf->buf, &ch, 1);

            /* Do input processing if any is enabled */

            if (class->tc_iflag & (INLCR | IGNCR | ICRNL))
            {
                /* \n -> \r or \r -> \n translation? */

                if ((ch == '\n') && (class->tc_iflag & INLCR))
                {
                    ch = '\r';
                }
                else if ((ch == '\r') && (class->tc_iflag & ICRNL))
                {
                    ch = '\n';
                }

                /* Discarding \r ? */

                if ((ch == '\r') & (class->tc_iflag & IGNCR))
                {
                    continue;
                }
            }

            /* Specifically not handled:
             *
             * All of the local modes; echo, line editing, etc.
             * Anything to do with break or parity errors.
             * ISTRIP - we should be 8-bit clean.
             * IUCLC - Not Posix
             * IXON/OXOFF - no xon/xoff flow control.
             */

            /* Store the received character */

            *buffer++ = ch;
            recvd++;

            if (class->tc_lflag & ECHO)
            {
                /* Check for the beginning of a VT100 escape sequence, 3 byte */

                if (ch == ASCII_ESC)
                {
                    /* Mark that we should skip 2 more bytes */

                    class->escape = 2;
                    continue;
                }
                else if (class->escape == 2 && ch != ASCII_LBRACKET)
                {
                    /* It's not an <esc>[x 3 byte sequence, show it */

                    class->escape = 0;
                }

                /* Echo if the character is not a control byte */

                if ((!iscntrl(ch & 0xff) || (ch == '\n')) && class->escape == 0)
                {
                    if (ch == '\n')
                    {
                        char_putxmitchar(dev, '\r', true);
                    }

                    char_putxmitchar(dev, ch, true);

                    /* Mark the tx buffer have echoed content here,
                     * to avoid the tx buffer is empty such as special escape
                     * sequence received, but enable the tx interrupt.
                     */

                    echoed = true;
                }

                /* Skipping character count down */

                if (class->escape > 0)
                {
                    class->escape--;
                }
            }
        }

#ifdef CONFIG_DEV_SERIAL_FULLBLOCKS
        /* No... then we would have to wait to get receive more data.
         * If the user has specified the O_NONBLOCK option, then just
         * return what we have.
         */

        else if ((filep->f_oflags & O_NONBLOCK) != 0)
        {
            /* If nothing was transferred, then return the -EAGAIN
             * error (not zero which means end of file).
             */

            if (recvd < 1)
            {
                recvd = -EAGAIN;
            }

            break;
        }
#else
        /* No... the circular buffer is empty.  Have we returned anything
         * to the caller?
         */

        else if (recvd > 0)
        {
            /* Yes.. break out of the loop and return the number of bytes
             * received up to the wait condition.
             */

            break;
        }

        else if (filep->f_inode == 0)
        {
            /* File has been closed.
             * Descriptor is not valid.
             */

            recvd = -EBADFD;
            break;
        }

        /* No... then we would have to wait to get receive some data.
         * If the user has specified the O_NONBLOCK option, then do not
         * wait.
         */

        else if ((filep->f_oflags & O_NONBLOCK) != 0)
        {
            /* Break out of the loop returning -EAGAIN */

            recvd = -EAGAIN;
            break;
        }
#endif

        /* Otherwise we are going to have to wait for data to arrive */

        else
        {
            /* Disable all interrupts and test again... */

            ttos_int_lock(flags);

            /* Disable Rx interrupts and test again... */

            char_disablerxint(dev);

            /* If the Rx ring buffer still empty?  Bytes may have been added
             * between the last time that we checked and when we disabled
             * interrupts.
             */

            if (circbuf_is_empty(&rxbuf->buf))
            {
                /* Yes.. the buffer is still empty.  We will need to wait for
                 * additional data to be received.
                 */

#ifdef CONFIG_SERIAL_RXDMA
                /* Notify DMA that there is free space in the RX buffer */

                char_dmarxfree(dev);
#endif
                /* Wait with the RX interrupt re-enabled.  All interrupts are
                 * disabled briefly to assure that the following operations
                 * are atomic.
                 */

                /* Re-enable UART Rx interrupts */

                char_enablerxint(dev);

                /* Check again if the RX buffer is empty.  The UART driver
                 * might have buffered data received between disabling the
                 * RX interrupt and entering the critical section.  Some
                 * drivers (looking at you, cdcacm...) will push the buffer
                 * to the receive queue during char_enablerxint().
                 * Just continue processing the RX queue if this happens.
                 */

                if (!circbuf_is_empty(&rxbuf->buf))
                {
                    ttos_int_unlock(flags);
                    continue;
                }

#ifdef CONFIG_SERIAL_REMOVABLE
                /* Check again if the removable device is still connected
                 * while we have interrupts off.  We do not want the transition
                 * to occur as a race condition before we begin the wait.
                 */

                if (class->disconnected)
                {
                    ret = -ENOTCONN;
                }
                else
#endif
                {
                    /* Now wait with the Rx interrupt enabled.  NuttX will
                     * automatically re-enable global interrupts when this
                     * thread goes to sleep.
                     */

                    class->minrecv = MIN(buflen - recvd, class->minread - recvd);
                    if (class->timeout)
                    {
                        /* timeout 单位是分秒 dsec */
                        uint64_t ticks = class->timeout * 100; /* 转为ms */
                        ticks = ticks * TTOS_GetSysClkRate() / MSEC_PER_SEC;

                        ret = TTOS_ObtainSema(class->recvsem, ticks);
                    }
                    else
                    {
                        ret = TTOS_ObtainSema(class->recvsem, TTOS_WAIT_FOREVER);
                    }
                }

                ttos_int_unlock(flags);

                /* Was a signal received while waiting for data to be
                 * received?  Was a removable device disconnected while
                 * we were waiting?
                 */
                if (ret != TTOS_OK)
                {
                    TTOS_ReleaseMutex(class->recv.lock);
                    return -ttos_ret_to_errno(ret);
                }
#ifdef CONFIG_SERIAL_REMOVABLE
                if (ret < 0 || dev->disconnected)
#else
                if (ret < 0)
#endif
                {
                    /* POSIX requires that we return after a signal is
                     * received.
                     * If some bytes were read, we need to return the
                     * number of bytes read; if no bytes were read, we
                     * need to return -1 with the errno set correctly.
                     */

                    if (recvd == 0)
                    {
                        /* No bytes were read, return -EINTR
                         * (the VFS layer will set the errno value
                         * appropriately).
                         */

#ifdef CONFIG_SERIAL_REMOVABLE
                        recvd = dev->disconnected ? -ENOTCONN : ret;
#else
                        recvd = ret;
#endif
                    }

                    break;
                }
            }
            else
            {
                /* No... the ring buffer is no longer empty.  Just re-enable Rx
                 * interrupts and accept the new data on the next time through
                 * the loop.
                 */

                ttos_int_unlock(flags);

                char_enablerxint(dev);
            }
        }
    }

    if (echoed)
    {
#ifdef CONFIG_SERIAL_TXDMA
        char_dmatxavail(dev);
#endif
        char_enabletxint(dev);
    }

#ifdef CONFIG_SERIAL_RXDMA
    /* Notify DMA that there is free space in the RX buffer */
    ttos_int_lock(flags);
    char_dmarxfree(dev);
    ttos_int_unlock(flags);
#endif

    /* RX interrupt could be disabled by RX buffer overflow. Enable it now. */

    char_enablerxint(dev);

#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
    /* How many bytes are now buffered */

    rxbuf = &dev->recv;
    if (rxbuf->head >= rxbuf->tail)
    {
        nbuffered = rxbuf->head - rxbuf->tail;
    }
    else
    {
        nbuffered = rxbuf->size - rxbuf->tail + rxbuf->head;
    }

    /* Is the level now below the watermark level that we need to report? */

    watermark = (CONFIG_SERIAL_IFLOWCONTROL_LOWER_WATERMARK * rxbuf->size) / 100;
    if (nbuffered <= watermark)
    {
        /* Let the lower level driver know that the watermark level has been
         * crossed.  It will probably deactivate RX flow control.
         */

        char_rxflowcontrol(dev, nbuffered, false);
    }
#else
    /* Is the RX buffer empty? */

    if (circbuf_is_empty(&rxbuf->buf))
    {
        /* Deactivate RX flow control. */

        (void)char_rxflowcontrol(dev, 0, false);
    }
#endif

    TTOS_ReleaseMutex(class->recv.lock);
    return recvd;
}

/****************************************************************************
 * Name: char_write
 ****************************************************************************/

static ssize_t char_write(struct file *filep, const char *buffer, size_t buflen)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    ssize_t nwritten = buflen;
    bool oktoblock;
    int ret;
    char ch;
    struct char_class_type *class = (struct char_class_type *)dev->class;

    /* We may receive serial writes through this path from interrupt handlers
     * and from debug output in the IDLE task!  In these cases, we will need to
     * do things a little differently.
     */

    if (ttosIsISR() || ttosIsIdleTask(ttosGetRunningTask()))
    {
        long flags;

#ifdef CONFIG_SERIAL_REMOVABLE
        /* If the removable device is no longer connected, refuse to write to
         * the device.
         */

        if (class->disconnected)
        {
            return -ENOTCONN;
        }
#endif

        ttos_int_lock(flags);
        ret = char_irqwrite(dev, buffer, buflen);
        ttos_int_unlock(flags);

        return ret;
    }

    /* Only one user can access dev->xmit.head at a time */

    ret = TTOS_ObtainMutex(class->xmit.lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        /* A signal received while waiting for access to the xmit.head will
         * abort the transfer.  After the transfer has started, we are
         * committed and signals will be ignored.
         */
        KLOG_E("TTOS_ObtainMutex ret %d", ret);
        return -EAGAIN;
    }

#ifdef CONFIG_SERIAL_REMOVABLE
    /* If the removable device is no longer connected, refuse to write to the
     * device.  This check occurs after taking the xmit.lock because the
     * disconnection event might have occurred while we were waiting for
     * access to the transmit buffers.
     */

    if (class->disconnected)
    {
        TTOS_ReleaseMutex(class->xmit.lock);
        return -ENOTCONN;
    }
#endif

    /* Can the following loop block, waiting for space in the TX
     * buffer?
     */

    oktoblock = ((filep->f_oflags & O_NONBLOCK) == 0);

    /* Loop while we still have data to copy to the transmit buffer.
     * we add data to the head of the buffer; uart_xmitchars takes the
     * data from the end of the buffer.
     */

    char_disabletxint(dev);
    for (; buflen; buflen--)
    {
        ch = *buffer++;
        ret = 0;

        /* Do output post-processing */

        if ((class->tc_oflag & OPOST) != 0)
        {
            /* Mapping CR to NL? */

            if ((ch == '\r') && (class->tc_oflag & OCRNL) != 0)
            {
                ch = '\n';
            }

            /* Are we interested in newline processing? */

            if ((ch == '\n') && (class->tc_oflag & (ONLCR | ONLRET)) != 0)
            {
                ret = char_putxmitchar(dev, '\r', oktoblock);
            }

            /* Specifically not handled:
             *
             * OXTABS - primarily a full-screen terminal optimization
             * ONOEOT - Unix interoperability hack
             * OLCUC  - Not specified by POSIX
             * ONOCR  - low-speed interactive optimization
             */
        }

        /* Put the character into the transmit buffer */

        if (ret >= 0)
        {
            ret = char_putxmitchar(dev, ch, oktoblock);
        }

        /* char_putxmitchar() might return an error under one of two
         * conditions:  (1) The wait for buffer space might have been
         * interrupted by a signal (ret should be -EINTR), (2) if
         * CONFIG_SERIAL_REMOVABLE is defined, then char_putxmitchar()
         * might also return if the serial device was disconnected
         * (with -ENOTCONN), or (3) if O_NONBLOCK is specified, then
         * then char_putxmitchar() might return -EAGAIN if the output
         * TX buffer is full.
         */

        if (ret < 0)
        {
            /* POSIX requires that we return -1 and errno set if no data was
             * transferred.  Otherwise, we return the number of bytes in the
             * interrupted transfer.
             */

            if (buflen < (size_t)nwritten)
            {
                /* Some data was transferred.  Return the number of bytes that
                 * were successfully transferred.
                 */

                nwritten -= buflen;
            }
            else
            {
                /* No data was transferred. Return the negated errno value.
                 * The VFS layer will set the errno value appropriately).
                 */

                nwritten = ret;
            }

            break;
        }
    }

    if (!circbuf_is_empty(&class->xmit.buf))
    {
#ifdef CONFIG_SERIAL_TXDMA
        char_dmatxavail(dev);
#endif
        char_enabletxint(dev);
    }

    TTOS_ReleaseMutex(class->xmit.lock);
    return nwritten;
}

/****************************************************************************
 * Name: char_ioctl
 ****************************************************************************/

static int char_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    struct char_class_type *class = (struct char_class_type *)dev->class;

    /* Handle TTY-level IOCTLs here */

    /* Let low-level driver handle the call first */

    int ret = class->ops->ioctl ? class->ops->ioctl(filep, cmd, arg) : -ENOTTY;

    /* The device ioctl() handler returns -ENOTTY when it doesn't know
     * how to handle the command. Check if we can handle it here.
     */

    if (ret == -ENOTTY)
    {
        switch (cmd)
        {
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
            /* 获取前台进程组 */
        case TIOCGPGRP:
        {
            int *pgrp = (int *)arg;

            pcb_t pcb = ttosProcessSelf();
            if (get_process_ctty(pcb) == inode)
            {
                if (class->gpid < 0)
                {
                    /* 如果没有前台进程组, 且当前进程的控制终端是当前tty，则tcgetpgrp()
                     * 应返回一个大于 1 的值，该值不匹配任何现有的进程组 ID */
                    *pgrp = 65536; // 65536 is a common value used to indicate no foreground process
                                   // group
                }
                else
                {
                    *pgrp = class->gpid;
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
            /* 设置前台进程组 */
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
            class->gpid = gpid;
            ret = 0;
        }
        break;
            /* 获取 窗口大小 */
        case TIOCGWINSZ:
        {
            struct winsize *pw = (struct winsize *)((uintptr_t)arg);

            /* Get row/col from the private data */
            *pw = class->winsize;
            ret = 0;
        }
        break;
        case TIOCSWINSZ:
        {
            struct winsize *pw = (struct winsize *)((uintptr_t)arg);

            /* Set row/col from the private data */
            if (memcmp(pw, &class->winsize, sizeof(class->winsize)))
            {
                class->winsize = *pw;
                if (class->gpid != INVALID_PROCESS_ID)
                {
                    kernel_signal_send(class->gpid, TO_PGROUP, SIGWINCH, SI_KERNEL, 0);
                }
            }
            ret = 0;
        }
        break;
        case FIOASYNC:
        {
            class->aio.aio_enable = !!arg;
            ret = 0;
        }
        break;
        case FIOGETOWN:
        {
            if (arg)
            {
                *(int *)arg = class->aio.pid;
                ret = 0;
            }
        }
        break;
        case FIOSETOWN:
        {
            if (arg)
            {
                class->aio.pid = (int)arg;
                ret = 0;
            }
            else
            {
                ret = -EINVAL;
            }
        }
        break;
        case FIONREAD:
        {
            int count;
            long flags;
            ttos_int_lock(flags);

            /* Determine the number of bytes available in the RX buffer */
            count = circbuf_used(&class->recv.buf);

            ttos_int_unlock(flags);

            *(int *)((uintptr_t)arg) = count;
            ret = 0;
        }
        break;

            /* Get the number of bytes that have been written to the TX
             * buffer.
             */

        case F_DUPFD_CLOEXEC:

            KLOG_E("fail at %s:%d\n", __FILE__, __LINE__);
            ret = 0;
            break;

        case F_GETFD:
            KLOG_E("fail at %s:%d\n", __FILE__, __LINE__);
            ret = 0;
            break;
        case F_SETFD:
            KLOG_E("fail at %s:%d\n", __FILE__, __LINE__);
            ret = 0;
            break;
        case TCFLSH:
        {
            /* Empty the tx/rx buffers */

            long flags;
            ttos_int_lock(flags);

            if (arg == TCIFLUSH || arg == TCIOFLUSH)
            {
                circbuf_reset(&class->recv.buf);

                /* De-activate RX flow control. */

                (void)char_rxflowcontrol(dev, 0, false);
            }

            if (arg == TCOFLUSH || arg == TCIOFLUSH)
            {
                circbuf_reset(&class->xmit.buf);

                /* Inform any waiters there there is space available. */

                char_datasent(dev);
            }

            ttos_int_unlock(flags);
            ret = 0;
        }
        break;
        case TCSBRK:
        {
            /* Non-standard Break specifies duration in milliseconds */
            /* 似乎需要驱动支持 临时修改 */
            // ret = char_tcsendbreak (dev, filep, arg);
            /* MUSL 通过TCSBRK 进行tcdrain */
            ret = char_tcdrain(dev, arg * 100);
        }
        break;

        case TCSBRKP:
        {
            /* POSIX Break specifies duration in units of 100ms */

            ret = char_tcsendbreak(dev, filep, arg * 100);
        }
        break;

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP)
            /* Make the controlling terminal of the calling process */

        case TIOCSCTTY:
        {
            ret = tty_set_ctty(inode, ttosProcessSelf());
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
#endif
        }
    }

    /* Append any higher level TTY flags */

    if (ret == 0 || ret == -ENOTTY)
    {
        switch (cmd)
        {
        case TCGETS:
        {
            struct termios *termiosp = (struct termios *)(uintptr_t)arg;

            if (!termiosp)
            {
                ret = -EINVAL;
                break;
            }

            /* And update with flags from this layer */

            termiosp->c_iflag = class->tc_iflag;
            termiosp->c_oflag = class->tc_oflag;
            termiosp->c_lflag = class->tc_lflag;
            termiosp->c_line = class->c_line;
            memcpy(termiosp->c_cc, class->c_cc, NCCS);

            ret = 0;
        }
        break;

        case TCSETS + TCSAFLUSH:
            ret = char_ioctl(filep, TCFLSH, TCIOFLUSH);
            if (ret < 0)
            {
                break;
            }
            /* 这里就是不 break的 因为还要执行后面的 TCSETS + TCSANOW */
        case TCSETS + TCSADRAIN:
            ret = char_tcdrain(dev, arg * 100);
            if (ret < 0)
            {
                break;
            }
            /* 这里就是不 break的 因为还要执行后面的 TCSETS + TCSANOW */
        case TCSETS + TCSANOW:
        {
            struct termios *termiosp = (struct termios *)(uintptr_t)arg;

            if (!termiosp)
            {
                ret = -EINVAL;
                break;
            }

            /* Update the flags we keep at this layer */

            class->tc_iflag = termiosp->c_iflag;
            class->tc_oflag = termiosp->c_oflag;
            class->tc_lflag = termiosp->c_lflag;
            class->c_line = termiosp->c_line;
            memcpy(class->c_cc, termiosp->c_cc, NCCS);
            class->timeout = termiosp->c_cc[VTIME];
            class->minread = termiosp->c_cc[VMIN];
            ret = 0;
        }
        break;
        }
    }

    return ret;
}

/****************************************************************************
 * Name: char_poll
 ****************************************************************************/

static int char_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    struct inode *inode = filep->f_inode;
    struct device *dev = inode->i_private;
    short eventset;
    int ret;
    int i;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* Some sanity checking */

#ifdef CONFIG_DEBUG_FEATURES
    if (dev == NULL || fds == NULL)
    {
        return -ENODEV;
    }
#endif

    /* Are we setting up the poll?  Or tearing it down? */

    ret = TTOS_ObtainMutex(class->polllock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        /* A signal received while waiting for access to the poll data
         * will abort the operation.
         */

        return -EAGAIN;
    }

    ttosDisableTaskDispatchWithLock();

    if (setup)
    {
        /* This is a request to set up the poll.  Find an available
         * slot for the poll structure reference
         */

        for (i = 0; i < CONFIG_SERIAL_NPOLLWAITERS; i++)
        {
            /* Find an available slot */

            if (!class->fds[i])
            {
                /* Bind the poll structure and this slot */

                class->fds[i] = fds;
                fds->priv = &class->fds[i];
                break;
            }
        }

        if (i >= CONFIG_SERIAL_NPOLLWAITERS)
        {
            fds->priv = NULL;
            ret = -EBUSY;
            goto errout;
        }

        ttosEnableTaskDispatchWithLock();
        TTOS_ReleaseMutex(class->polllock);

        /* Should we immediately notify on any of the requested events?
         * First, check if the xmit buffer is full.
         *
         * Get exclusive access to the xmit buffer indices.
         * NOTE: that we do not let this wait be interrupted by a signal
         * (we probably should, but that would be a little awkward).
         */

        eventset = 0;
        TTOS_ObtainMutex(class->xmit.lock, TTOS_WAIT_FOREVER);

        if (!circbuf_is_full(&class->xmit.buf))
        {
            eventset |= POLLOUT;
        }

        TTOS_ReleaseMutex(class->xmit.lock);

        /* Check if the receive buffer is empty.
         *
         * Get exclusive access to the recv buffer indices.
         * NOTE: that we do not let this wait be interrupted by a signal
         * (we probably should, but that would be a little awkward).
         */

        TTOS_ObtainMutex(class->recv.lock, TTOS_WAIT_FOREVER);
        if (!circbuf_is_empty(&class->recv.buf))
        {
            eventset |= POLLIN;
        }

        TTOS_ReleaseMutex(class->recv.lock);

#ifdef CONFIG_SERIAL_REMOVABLE
        /* Check if a removable device has been disconnected. */

        if (dev->disconnected)
        {
            eventset |= (POLLERR | POLLHUP);
        }
#endif

        kpoll_notify(class->fds, CONFIG_SERIAL_NPOLLWAITERS, eventset, &class->aio);
    }
    else
    {
        if (fds->priv != NULL)
        {
            /* This is a request to tear down the poll. */

            struct kpollfd **slot = (struct kpollfd **)fds->priv;

            /* Remove all memory of the poll setup */

            *slot = NULL;
            fds->priv = NULL;
        }
        ttosEnableTaskDispatchWithLock();
        TTOS_ReleaseMutex(class->polllock);
    }

    return ret;

errout:
    ttosEnableTaskDispatchWithLock();
    TTOS_ReleaseMutex(class->polllock);
    return ret;
}

/****************************************************************************
 * Name: uart_nxsched_foreach_cb
 ****************************************************************************/

#ifdef CONFIG_TTY_LAUNCH
static void uart_launch_foreach(struct tcb_s *tcb, void *arg)
{
#ifdef CONFIG_TTY_LAUNCH_ENTRY
    if (!strcmp(tcb->name, CONFIG_TTY_LAUNCH_ENTRYNAME))
#else
    if (!strcmp(tcb->name, CONFIG_TTY_LAUNCH_FILEPATH))
#endif
    {
        *(int *)arg = 1;
    }
}

static void uart_launch_worker(void *arg)
{
#ifdef CONFIG_TTY_LAUNCH_ARGS
    char *const argv[] = {
        CONFIG_TTY_LAUNCH_ARGS,
        NULL,
    };
#else
    char *const *argv = NULL;
#endif
    int found = 0;

    nxsched_foreach(uart_launch_foreach, &found);
    if (!found)
    {
        posix_spawnattr_t attr;

        posix_spawnattr_init(&attr);
        attr.priority = CONFIG_TTY_LAUNCH_PRIORITY;
        attr.stacksize = CONFIG_TTY_LAUNCH_STACKSIZE;

#ifdef CONFIG_TTY_LAUNCH_ENTRY
        task_spawn(CONFIG_TTY_LAUNCH_ENTRYNAME, CONFIG_TTY_LAUNCH_ENTRYPOINT, NULL, &attr, argv,
                   NULL);
#else
        exec_spawn(CONFIG_TTY_LAUNCH_FILEPATH, argv, NULL, NULL, 0, NULL, &attr);
#endif
        posix_spawnattr_destroy(&attr);
    }
}

static void uart_launch(void)
{
    work_queue(HPWORK, &g_serial_work, uart_launch_worker, NULL, 0);
}
#endif

static void char_wakeup(SEMA_ID sem)
{
    int sval = sem->semaControlValue;

    /* Yes... wake up all waiting threads */

    while (sval++ < 1)
    {
        TTOS_ReleaseSema(sem);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: char_register
 *
 * Description:
 *   Register serial console and serial ports.
 *
 ****************************************************************************/

int char_register(const char *path, struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP)
    /* Initialize  of the task that will receive SIGINT signals. */

    class->pid = INVALID_PROCESS_ID;
    class->gpid = INVALID_PROCESS_ID;
#endif

    /* If this UART is a serial console */

    if (class->isconsole)
    {
        /* Enable signals and echo by default */

        class->tc_lflag |= TTYDEF_LFLAG;

        /* Enable \n -> \r\n translation for the console */

        class->tc_oflag = TTYDEF_OFLAG;

        /* Convert CR to LF by default for console */

        class->tc_iflag |= TTYDEF_IFLAG;

        /* Clear escape counter */

        class->escape = 0;
    }

    /* Initialize mutex & semaphores */

    TTOS_CreateMutex(1, 0, &class->xmit.lock);
    TTOS_CreateMutex(1, 0, &class->recv.lock);
    TTOS_CreateMutex(1, 0, &class->closelock);

    TTOS_CreateSemaEx(0, &class->xmitsem);
    TTOS_CreateSemaEx(0, &class->recvsem);
    TTOS_CreateMutex(1, 0, &class->polllock);

    class->timeout = 0;
    class->minread = 1;

    /* Register the serial driver */

    return device_bind_path(dev, path, &g_serialops, 0666);
}

/****************************************************************************
 * Name: char_datareceived
 *
 * Description:
 *   This function is called from uart_recvchars when new serial data is
 *   place in the driver's circular buffer.  This function will wake-up any
 *   stalled read() operations that are waiting for incoming data.
 *
 ****************************************************************************/

void char_datareceived(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* Notify all poll/select waiters that they can read from the recv buffer */

    kpoll_notify(class->fds, CONFIG_SERIAL_NPOLLWAITERS, POLLIN, &class->aio);

    /* Is there a thread waiting for read data?  */

    char_wakeup(class->recvsem);

#if defined(CONFIG_PM) && defined(CONFIG_SERIAL_CONSOLE)
    /* Call pm_activity when characters are received on the console device */

    if (class->isconsole)
    {
        pm_activity(CONFIG_SERIAL_PM_ACTIVITY_DOMAIN, CONFIG_SERIAL_PM_ACTIVITY_PRIORITY);
    }
#endif
}

/****************************************************************************
 * Name: char_datasent
 *
 * Description:
 *   This function is called from uart_xmitchars after serial data has been
 *   sent, freeing up some space in the driver's circular buffer. This
 *   function will wake-up any stalled write() operations that was waiting
 *   for space to buffer outgoing data.
 *
 ****************************************************************************/

void char_datasent(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    /* Notify all poll/select waiters that they can write to xmit buffer */

    kpoll_notify(class->fds, CONFIG_SERIAL_NPOLLWAITERS, POLLOUT, &class->aio);

    /* Is there a thread waiting for space in xmit.buffer?  */

    char_wakeup(class->xmitsem);
}

/****************************************************************************
 * Name: uart_connected
 *
 * Description:
 *   Serial devices (like USB serial) can be removed.
 *   In that case, the "upper half" serial driver must be informed that there
 *   is no longer a valid serial channel associated with the driver.
 *
 *   In this case, the driver will terminate all pending transfers wint
 *   ENOTCONN and will refuse all further transactions while the "lower half"
 *   is disconnected.
 *   The driver will continue to be registered, but will be in an unusable
 *   state.
 *
 *   Conversely, the "upper half" serial driver needs to know when the serial
 *   device is reconnected so that it can resume normal operations.
 *
 * Assumptions/Limitations:
 *   This function may be called from an interrupt handler.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_REMOVABLE
void uart_connected(struct device *dev, bool connected)
{
    irqstate_t flags;

    /* Is the device disconnected?  Interrupts are disabled because this
     * function may be called from interrupt handling logic.
     */

    flags = enter_critical_section();
    dev->disconnected = !connected;
    if (!connected)
    {
        /* Notify all poll/select waiters that a hangup occurred */

        kpoll_notify(dev->fds, CONFIG_SERIAL_NPOLLWAITERS, POLLERR | POLLHUP);

        /* Yes.. wake up all waiting threads.  Each thread should detect the
         * disconnection and return the ENOTCONN error.
         */

        /* Is there a thread waiting for space in xmit.buffer?  */

        char_wakeup(&dev->xmitsem);

        /* Is there a thread waiting for read data?  */

        char_wakeup(&dev->recvsem);
    }

    ttos_int_unlock(flags);
}
#endif

/****************************************************************************
 * Name: char_reset_sem
 *
 * Description:
 *   This function is called when need reset uart semaphore, this may used in
 *   kill one process, but this process was reading/writing with the
 *   semaphore.
 *
 ****************************************************************************/

void char_reset_sem(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    TTOS_ResetSema(0, class->xmitsem);
    TTOS_ResetSema(0, class->recvsem);
}

void char_xmitchars(struct device *dev)
{
    uint16_t nbytes = 0;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    long flags;
    char ch;
    spin_lock_irqsave(&class->xmit_spinlock, flags);

    /* Send while we still have data in the TX buffer & room in the fifo */

    while (!circbuf_is_empty(&class->xmit.buf) && char_txready(dev))
    {
        /* Send the next byte */
        circbuf_read(&class->xmit.buf, &ch, 1);
        char_send(dev, ch);
        nbytes++;
    }

    /* When all of the characters have been sent from the buffer disable the TX
     * interrupt.
     *
     * Potential bug?  If nbytes == 0 && (dev->xmit.head == dev->xmit.tail) &&
     * dev->xmitwaiting == true, then disabling the TX interrupt will leave
     * the uart_write() logic waiting to TX to complete with no TX interrupts.
     * Can that happen?
     */

    if (circbuf_is_empty(&class->xmit.buf))
    {
        char_disabletxint(dev);
    }

    /* If any bytes were removed from the buffer, inform any waiters that
     * there is space available.
     */

    spin_unlock_irqrestore(&class->xmit_spinlock, flags);

    if (nbytes)
    {
        char_datasent(dev);
    }
}

/****************************************************************************
 * Name: char_recvchars
 *
 * Description:
 *   This function is called from the UART interrupt handler when an
 *   interrupt is received indicating that are bytes available in the
 *   receive FIFO.  This function will add chars to head of receive buffer.
 *   Driver read() logic will take characters from the tail of the buffer.
 *
 ****************************************************************************/

void char_recvchars(struct device *dev)
{
#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
    unsigned int watermark;
#endif
    struct char_class_type *class = (struct char_class_type *)dev->class;
    struct char_buffer_s *rxbuf = &class->recv;
    unsigned int status;
#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    int signo = 0;
    bool cad = 0;
#endif
    uint16_t nbytes = 0;

#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
    /* Pre-calculate the watermark level that we will need to test against. */

    watermark = (CONFIG_SERIAL_IFLOWCONTROL_UPPER_WATERMARK * rxbuf->size) / 100;
#endif

    /* Loop putting characters into the receive buffer until there are no
     * further characters to available.
     */

    while (char_rxavailable(dev))
    {
        char ch;

#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
        unsigned int nbuffered;

        /* How many bytes are buffered */

        if (rxbuf->head >= rxbuf->tail)
        {
            nbuffered = rxbuf->head - rxbuf->tail;
        }
        else
        {
            nbuffered = rxbuf->size - rxbuf->tail + rxbuf->head;
        }

        /* Is the level now above the watermark level that we need to report? */

        if (nbuffered >= watermark)
        {
            /* Let the lower level driver know that the watermark level has
             * been crossed.  It will probably activate RX flow control.
             */

            if (char_rxflowcontrol(dev, nbuffered, true))
            {
                /* Low-level driver activated RX flow control, exit loop now. */

                break;
            }
        }
#else
        /* Check if RX buffer is full and allow serial low-level driver to
         * pause processing. This allows proper utilization of hardware flow
         * control.
         */

        if (circbuf_is_full(&rxbuf->buf))
        {
            if (char_rxflowcontrol(dev, circbuf_size(&rxbuf->buf), true))
            {
                /* Low-level driver activated RX flow control, exit loop now. */

                break;
            }
        }
#endif

        /* Get this next character from the hardware */

        ch = char_receive(dev, &status);

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
        signo = tty_check_special(class->tc_lflag, &ch, 1);
        // cad = (ch == 27);
#endif

        /* If the RX buffer becomes full, then the serial data is discarded.
         * This is necessary because on most serial hardware, you must read
         * the data in order to clear the RX interrupt. An option on some
         * hardware might be to simply disable RX interrupts until the RX
         * buffer becomes non-FULL.  However, that would probably just cause
         * the overrun to occur in hardware (unless it has some large internal
         * buffering).
         */

        if (!circbuf_is_full(&rxbuf->buf))
        {
            circbuf_write(&rxbuf->buf, &ch, 1);
            nbytes++;
        }
    }

#ifdef CONFIG_SERIAL_TERMIOS
    if (nbytes >= class->minrecv)
#else
    if (nbytes)
#endif
    {
        char_datareceived(dev);
    }

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    /* Send the signal if necessary */
    if (cad)
    {
        if (CAD_is_signal_to_init())
        {
            kernel_signal_send(1, TO_PGROUP, SIGINT, SI_KERNEL, 0);
        }
        else
        {
            cpu_reset(cpu_boot_dev_get());
        }
    }
    if (signo != 0)
    {
        if (class->gpid != INVALID_PROCESS_ID)
        {
            kernel_signal_send(class->gpid, TO_PGROUP, signo, SI_KERNEL, 0);
        }
        char_reset_sem(dev);
    }
#endif
}
