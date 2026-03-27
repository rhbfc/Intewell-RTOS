/****************************************************************************
 *
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <fs/kpoll.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <system/types.h>
// #include <sched.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>

#include "pipe_common.h"

#include <ttos.h>
#include <ttosBase.h>
#include <restart.h>

#define KLOG_TAG "PIPE"
#include <klog.h>

#ifdef CONFIG_PIPES

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pipecommon_wakeup
 ****************************************************************************/
static void pipecommon_wakeup(SEMA_ID *sem)
{
    while ((*sem)->semaControlValue <= 0)
    {
        TTOS_ReleaseSema(*sem);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pipecommon_allocdev
 ****************************************************************************/
struct pipe_dev_s *pipecommon_allocdev(size_t bufsize)
{
    struct pipe_dev_s *dev = NULL;

    if (bufsize <= CONFIG_DEV_PIPE_MAXSIZE)
    {
        /* Allocate a private structure to manage the pipe */

        dev = malloc(sizeof(struct pipe_dev_s));
        if (dev)
        {
            /* Initialize the private structure */

            memset(dev, 0, sizeof(struct pipe_dev_s));
            TTOS_CreateMutex(1, 0, &dev->d_bflock);
            TTOS_CreateSemaEx(0, &dev->d_rdsem);
            TTOS_CreateSemaEx(0, &dev->d_wrsem);
            dev->d_bufsize = bufsize;
        }
    }

    return dev;
}

/****************************************************************************
 * Name: pipecommon_freedev
 ****************************************************************************/

void pipecommon_freedev(struct pipe_dev_s *dev)
{
    TTOS_DeleteMutex(dev->d_bflock);
    TTOS_DeleteSema(dev->d_rdsem);
    TTOS_DeleteSema(dev->d_wrsem);
    free(dev);
}

/****************************************************************************
 * Name: pipecommon_open
 ****************************************************************************/

int pipecommon_open(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    int ret;

    /* Make sure that we have exclusive access to the device structure.  The
     * pthread_mutex_lock() call should fail if we are awakened by a signal or
     * if the thread was canceled.
     */

    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* If this the first reference on the device, then allocate the buffer.
     * In the case of policy 1, the buffer already be present when the pipe
     * is first opened.
     */
    // inode_lock ();//use i_crefs need lock
    // inode->i_crefs == 1 && //2 file open the same time will add i_crefs to 2
    if (!circbuf_is_init(&dev->d_buffer))
    {
        dev->aio.aio_enable = false;
        dev->aio.pid = -1;
        ret = circbuf_init(&dev->d_buffer, NULL, dev->d_bufsize);
        if (ret < 0)
        {
            TTOS_ReleaseMutex(dev->d_bflock);
            // inode_unlock ();
            return ret;
        }
    }
    // inode_unlock ();

    /* If opened for writing, increment the count of writers on the pipe
     * instance.
     */

    if ((filep->f_oflags & O_ACCMODE) == O_WRONLY)
    {
        dev->d_nwriters++;

        /* If this is the first writer, then the n-readers semaphore
         * indicates the number of readers waiting for the first writer.
         * Wake them all up!
         */

        if (dev->d_nwriters == 1)
        {
            pipecommon_wakeup(&dev->d_rdsem);
        }
    }

    while ((filep->f_oflags & O_NONBLOCK) == 0 &&       /* Blocking */
           (filep->f_oflags & O_ACCMODE) == O_WRONLY && /* Write-only */
           dev->d_nreaders < 1 &&                       /* No readers on the pipe */
           circbuf_is_empty(&dev->d_buffer))            /* Buffer is empty */
    {
        /* If opened for write-only, then wait for at least one reader
         * on the pipe.
         */

        TTOS_ReleaseMutex(dev->d_bflock);

        /* NOTE: d_wrsem is normally used to check if the write buffer is full
         * and wait for it being read and being able to receive more data. But,
         * until the first reader has opened the pipe, the meaning is different
         * and it is used prevent O_WRONLY open calls from returning until
         * there is at least one reader on the pipe.
         */

        ret = TTOS_ObtainSema(dev->d_wrsem, TTOS_WAIT_FOREVER);
        if (ret != TTOS_OK)
        {
            pipecommon_close(filep);
            if (ret == TTOS_SIGNAL_INTR)
            {
                /* If we were awakened by a signal, then return ERR_RESTART_IF_SIGNAL
                 * to indicate that the operation was interrupted.
                 */
                return -ERR_RESTART_IF_SIGNAL;
            }
            else
            {
                return -ttos_ret_to_errno(ret);
            }
        }

        /* The pthread_mutex_lock() call should fail if we are awakened by a
         * signal or if the task is canceled.
         */

        ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
        if (ret != TTOS_OK)
        {
            KLOG_E("pthread_mutex_lock failed: %d", ret);

            /* Immediately close the pipe that we just opened */

            pipecommon_close(filep);
            return -ttos_ret_to_errno(ret);
        }
    }

    /* If opened for reading, increment the count of reader on on the pipe
     * instance.
     */
    // if ((filep->f_oflags & O_RDOK) != 0)
    if ((filep->f_oflags & O_ACCMODE) == O_RDONLY)
    {
        dev->d_nreaders++;

        /* If this is the first reader, then the n-writers semaphore
         * indicates the number of writers waiting for the first reader.
         * Wake them all up.
         */

        if (dev->d_nreaders == 1)
        {
            pipecommon_wakeup(&dev->d_wrsem);
        }
    }

    while ((filep->f_oflags & O_NONBLOCK) == 0 &&       /* Blocking */
           (filep->f_oflags & O_ACCMODE) == O_RDONLY && /* Read-only */
           dev->d_nwriters < 1 &&                       /* No writers on the pipe */
           circbuf_is_empty(&dev->d_buffer))            /* Buffer is empty */
    {
        /* If opened for read-only, then wait for either at least one writer
         * on the pipe.
         */

        TTOS_ReleaseMutex(dev->d_bflock);

        /* NOTE: d_rdsem is normally used when the read logic waits for more
         * data to be written.  But until the first writer has opened the
         * pipe, the meaning is different: it is used prevent O_RDONLY open
         * calls from returning until there is at least one writer on the pipe.
         * This is required both by spec and also because it prevents
         * subsequent read() calls from returning end-of-file because there is
         * no writer on the pipe.
         */

        ret = TTOS_ObtainSema(dev->d_rdsem, TTOS_WAIT_FOREVER);
        if (ret != TTOS_OK)
        {
            pipecommon_close(filep);
            if (ret == TTOS_SIGNAL_INTR)
            {
                /* If we were awakened by a signal, then return ERR_RESTART_IF_SIGNAL
                 * to indicate that the operation was interrupted.
                 */
                return -ERR_RESTART_IF_SIGNAL;
            }
            else
            {
                return -ttos_ret_to_errno(ret);
            }
        }

        /* The pthread_mutex_lock() call should fail if we are awakened by a
         * signal or if the task is canceled.
         */

        ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
        if (ret != TTOS_OK)
        {
            KLOG_E("pthread_mutex_lock failed: %d", ret);

            /* Immediately close the pipe that we just opened */

            pipecommon_close(filep);
            return -ttos_ret_to_errno(ret);
        }
    }

    TTOS_ReleaseMutex(dev->d_bflock);
    return ret;
}

/****************************************************************************
 * Name: pipecommon_close
 ****************************************************************************/

int pipecommon_close(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    int ret;

    // DEBUGASSERT(dev && filep->f_inode->i_crefs > 0);

    /* Make sure that we have exclusive access to the device structure.
     * NOTE: close() is supposed to return EINTR if interrupted, however
     * I've never seen anyone check that.
     */

    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        /* Immediately close the pipe that we just opened */

        return -ttos_ret_to_errno(ret);
    }

    /* Decrement the number of references on the pipe.  Check if there are
     * still outstanding references to the pipe.
     */

    /* Check if the decremented inode reference count would go to zero */

    if (inode->i_crefs > 1)
    {
        /* More references.. If opened for writing, decrement the count of
         * writers on the pipe instance.
         */

        if ((filep->f_oflags & O_ACCMODE) == O_WRONLY)
        {
            /* If there are no longer any writers on the pipe, then notify all
             * of the waiting readers that they must return end-of-file.
             */

            if (--dev->d_nwriters <= 0)
            {
                /* Inform poll readers that other end closed. */

                kpoll_notify(dev->d_fds, CONFIG_DEV_PIPE_NPOLLWAITERS, POLLHUP, &dev->aio);

                pipecommon_wakeup(&dev->d_rdsem);
            }
        }

        /* If opened for reading, decrement the count of readers on the pipe
         * instance.
         */

        if ((filep->f_oflags & O_ACCMODE) == O_RDONLY)
        {
            if (--dev->d_nreaders <= 0)
            {
                if (PIPE_IS_POLICY_0(dev->d_flags))
                {
                    /* Inform poll writers that other end closed. */

                    kpoll_notify(dev->d_fds, CONFIG_DEV_PIPE_NPOLLWAITERS, POLLERR, &dev->aio);
                    pipecommon_wakeup(&dev->d_wrsem);
                }
            }
        }
    }

    /* What is the buffer management policy?  Do we free the buffer when the
     * last client closes the pipe policy 0, or when the buffer becomes empty.
     * In the latter case, the buffer data will remain valid and can be
     * obtained when the pipe is re-opened.
     */

    else if (PIPE_IS_POLICY_0(dev->d_flags) || circbuf_is_empty(&dev->d_buffer))
    {
        /* Policy 0 or the buffer is empty ... deallocate the buffer now. */

        circbuf_uninit(&dev->d_buffer);

        /* And reset all counts and indices */

        dev->d_nwriters = 0;
        dev->d_nreaders = 0;

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
        /* If, in addition, we have been unlinked, then also need to free the
         * device structure as well to prevent a memory leak.
         */

        if (PIPE_IS_UNLINKED(dev->d_flags))
        {
            pipecommon_freedev(dev);
            return 0;
        }
#endif
    }

    TTOS_ReleaseMutex(dev->d_bflock);
    return 0;
}

/****************************************************************************
 * Name: pipecommon_read
 ****************************************************************************/

ssize_t pipecommon_read(struct file *filep, char *buffer, size_t len)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    ssize_t nread = 0;
    int ret;

    // DEBUGASSERT(dev);

    if (len == 0)
    {
        return 0;
    }

    /* Make sure that we have exclusive access to the device structure */

    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* If the pipe is empty, then wait for something to be written to it */

    while (circbuf_is_empty(&dev->d_buffer))
    {
        /* If there are no writers on the pipe, then return end of file */

        if (dev->d_nwriters <= 0)
        {
            TTOS_ReleaseMutex(dev->d_bflock);
            return 0;
        }

        /* If O_NONBLOCK was set, then return EGAIN */

        if (filep->f_oflags & O_NONBLOCK)
        {
            TTOS_ReleaseMutex(dev->d_bflock);
            return -EAGAIN;
        }

        /* Otherwise, wait for something to be written to the pipe */

        TTOS_ReleaseMutex(dev->d_bflock);
        ret = TTOS_ObtainSema(dev->d_rdsem, TTOS_WAIT_FOREVER);
        if (ret != TTOS_OK || (ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER)) != TTOS_OK)
        {
            /* May fail because a signal was received or if the task was
             * canceled.
             */
            return ret == TTOS_SIGNAL_INTR ? -ERR_RESTART_IF_SIGNAL : -ttos_ret_to_errno(ret);
        }
    }

    /* Then return whatever is available in the pipe (which is at least one
     * byte).
     */

    nread = circbuf_read(&dev->d_buffer, buffer, len);

    /* Notify all poll/select waiters that they can write to the
     * FIFO when buffer can accept more than d_polloutthrd bytes.
     */

    if (circbuf_used(&dev->d_buffer) <= (dev->d_bufsize - dev->d_polloutthrd))
    {
        kpoll_notify(dev->d_fds, CONFIG_DEV_PIPE_NPOLLWAITERS, POLLOUT, &dev->aio);
    }

    /* Notify all waiting writers that bytes have been removed from the
     * buffer.
     */

    pipecommon_wakeup(&dev->d_wrsem);

    TTOS_ReleaseMutex(dev->d_bflock);

    return nread;
}

/****************************************************************************
 * Name: pipecommon_write
 ****************************************************************************/

ssize_t pipecommon_write(struct file *filep, const char *buffer, size_t len)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    ssize_t nwritten = 0;
    ssize_t last;
    int ret;

    /* Handle zero-length writes */

    if (len == 0)
    {
        return 0;
    }

    // DEBUGASSERT(up_interrupt_context() == false);

    /* Make sure that we have exclusive access to the device structure */
    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    /* Loop until all of the bytes have been written */

    last = 0;
    for (;;)
    {
        /* REVISIT:  "If all file descriptors referring to the read end of a
         * pipe have been closed, then a write will cause a SIGPIPE signal to
         * be generated for the calling process.  If the calling process is
         * ignoring this signal, then write(2) fails with the error EPIPE."
         */

        if (dev->d_nreaders <= 0)
        {
            TTOS_ReleaseMutex(dev->d_bflock);
            return nwritten == 0 ? -EPIPE : nwritten;
        }

        /* Would the next write overflow the circular buffer? */

        if (!circbuf_is_full(&dev->d_buffer))
        {
            /* Loop until all of the bytes have been written */

            nwritten += circbuf_write(&dev->d_buffer, buffer + nwritten, len - nwritten);

            if ((size_t)nwritten == len)
            {
                /* Notify all poll/select waiters that they can read from the
                 * FIFO when buffer used exceeds poll threshold.
                 */

                if (circbuf_used(&dev->d_buffer) > dev->d_pollinthrd)
                {
                    kpoll_notify(dev->d_fds, CONFIG_DEV_PIPE_NPOLLWAITERS, POLLIN, &dev->aio);
                }

                /* Yes.. Notify all of the waiting readers that more data is
                 * available.
                 */

                pipecommon_wakeup(&dev->d_rdsem);

                /* Return the number of bytes written */

                TTOS_ReleaseMutex(dev->d_bflock);
                return len;
            }
        }
        else
        {
            /* There is not enough room for the next byte.  Was anything
             * written in this pass?
             */

            if (last < nwritten)
            {
                /* Notify all poll/select waiters that they can read from the
                 * FIFO.
                 */

                kpoll_notify(dev->d_fds, CONFIG_DEV_PIPE_NPOLLWAITERS, POLLIN, &dev->aio);

                /* Yes.. Notify all of the waiting readers that more data is
                 * available.
                 */

                pipecommon_wakeup(&dev->d_rdsem);
            }

            last = nwritten;

            /* If O_NONBLOCK was set, then return partial bytes written or
             * EGAIN.
             */

            if (filep->f_oflags & O_NONBLOCK)
            {
                if (nwritten == 0)
                {
                    nwritten = -EAGAIN;
                }

                TTOS_ReleaseMutex(dev->d_bflock);
                return nwritten;
            }

            /* There is more to be written.. wait for data to be removed from
             * the pipe
             */

            TTOS_ReleaseMutex(dev->d_bflock);
            ret = TTOS_ObtainSema(dev->d_wrsem, TTOS_WAIT_FOREVER);
            if (ret != TTOS_OK ||
                (ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER)) != TTOS_OK)
            {
                /* May fail because a signal was received or if the task was
                 * canceled.
                 */
                return ret == TTOS_SIGNAL_INTR ? -ERR_RESTART_IF_SIGNAL : -ttos_ret_to_errno(ret);
            }
        }
    }
}

/****************************************************************************
 * Name: pipecommon_poll
 ****************************************************************************/

int pipecommon_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    short eventset;
    pipe_ndx_t nbytes;
    int ret;
    int i;

    DEBUGASSERT(dev && fds);

    /* Are we setting up the poll?  Or tearing it down? */

    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }
    
    ttosDisableTaskDispatchWithLock();

    if (setup)
    {
        /* This is a request to set up the poll.  Find an available
         * slot for the poll structure reference
         */

        for (i = 0; i < CONFIG_DEV_PIPE_NPOLLWAITERS; i++)
        {
            /* Find an available slot */

            if (!dev->d_fds[i])
            {
                /* Bind the poll structure and this slot */

                dev->d_fds[i] = fds;
                fds->priv = &dev->d_fds[i];
                break;
            }
        }

        if (i >= CONFIG_DEV_PIPE_NPOLLWAITERS)
        {
            fds->priv = NULL;
            ret = -EBUSY;
            goto errout;
        }

        /* Should immediately notify on any of the requested events?
         * First, determine how many bytes are in the buffer
         */

        nbytes = circbuf_used(&dev->d_buffer);

        /* Notify the POLLOUT event if the pipe buffer can accept
         * more than d_polloutthrd bytes, but only if
         * there is readers.
         */

        eventset = 0;
        if (((filep->f_oflags & O_ACCMODE) != O_RDONLY) &&
            nbytes < (dev->d_bufsize - dev->d_polloutthrd))
        {
            eventset |= POLLOUT;
        }

        /* Notify the POLLIN event if buffer used exceeds poll threshold */

        if (((filep->f_oflags & O_ACCMODE) != O_WRONLY) && (nbytes > dev->d_pollinthrd))
        {
            eventset |= POLLIN;
        }

        /* Notify the POLLHUP event if the pipe is empty and no writers */

        if (nbytes == 0 && dev->d_nwriters <= 0)
        {
            eventset |= POLLHUP;
        }

        /* Change POLLOUT to POLLERR, if no readers and policy 0. */

        if ((eventset & POLLOUT) && PIPE_IS_POLICY_0(dev->d_flags) && dev->d_nreaders <= 0)
        {
            eventset |= POLLERR;
        }

        kpoll_notify(&fds, 1, eventset, &dev->aio);
    }
    else
    {
        /* This is a request to tear down the poll. */

        struct kpollfd **slot = (struct kpollfd **)fds->priv;

#ifdef CONFIG_DEBUG_FEATURES
        if (!slot)
        {
            ret = -EIO;
            goto errout;
        }
#endif

        /* Remove all memory of the poll setup */

        *slot = NULL;
        fds->priv = NULL;
    }

errout:
    ttosEnableTaskDispatchWithLock();
    TTOS_ReleaseMutex(dev->d_bflock);
    return ret;
}

/****************************************************************************
 * Name: pipecommon_ioctl
 ****************************************************************************/

int pipecommon_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    int ret = -EINVAL;

    if (dev == NULL)
    {
        return -EBADF;
    }

    ret = TTOS_ObtainMutex(dev->d_bflock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    switch (cmd)
    {
    case FIOASYNC:
    {
        dev->aio.aio_enable = !!arg;
        ret = 0;
    }
    break;
    case FIONBIO:
        ret = 0;
        break;
    case FIOGETOWN:
    {
        if (arg)
        {
            *(int *)arg = dev->aio.pid;
            ret = 0;
        }
    }
    break;
    case FIOSETOWN:
    {
        if (arg)
        {
            dev->aio.pid = (int)arg;
            ret = 0;
        }
        else
        {
            ret = -EINVAL;
        }
    }
    break;
    default:
        ret = -ENOTTY;
        break;
    }

    TTOS_ReleaseMutex(dev->d_bflock);

    return ret;
}

/****************************************************************************
 * Name: pipecommon_unlink
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
int pipecommon_unlink(struct inode *inode)
{
    struct pipe_dev_s *dev;

    // DEBUGASSERT(inode->i_private);
    dev = inode->i_private;

    /* Mark the pipe unlinked */

    PIPE_UNLINK(dev->d_flags);

    /* Are the any open references to the driver? */

    if (inode->i_crefs == 1)
    {
        /* No.. free the buffer (if there is one) */

        circbuf_uninit(&dev->d_buffer);

        /* And free the device structure. */

        pipecommon_freedev(dev);
    }

    return 0;
}
#endif

#endif /* CONFIG_PIPES */
