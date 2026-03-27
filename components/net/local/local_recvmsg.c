/****************************************************************************
 * net/local/local_recvmsg.c
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

#include "local.h"

#define KLOG_TAG "AF_LOCAL"
#include <klog.h>

/****************************************************************************
 * Name: psock_fifo_read
 *
 * Description:
 *   A thin layer around local_fifo_read that handles socket-related loss-of-
 *   connection events.
 *
 ****************************************************************************/

static int psock_fifo_read(struct socket *psock, void *buf, size_t *readlen, int flags, bool once)
{
    struct local_sock_s *lsock;
    int ret;

    if (psock == NULL)
    {
        return -EINVAL;
    }
    lsock = psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }

    ret = local_fifo_read(&lsock->infile, buf, readlen, once);
    if (ret < 0)
    {
        /* -ECONNRESET is a special case.  We may or not have received
         * data, then the peer closed the connection.
         */

        if (ret == -ECONNRESET)
        {
            KLOG_W("ERROR: Lost connection: %d", ret);

            /* Report an ungraceful loss of connection.  This should
             * eventually be reported as ENOTCONN.
             */

            lsock->flags &= ~(_SF_CONNECTED | _SF_CLOSED);
            lsock->state = LOCAL_STATE_DISCONNECTED;

            /* Did we receive any data? */

            if (*readlen <= 0)
            {
                /* No.. return the ECONNRESET error now.  Otherwise,
                 * process the received data and return ENOTCONN the
                 * next time that psock_recvfrom() is called.
                 */

                return ret;
            }
        }
        else
        {
            if (ret == -EAGAIN)
            {
                KLOG_W("WARNING: Failed to read packet: %d", ret);
            }
            else
            {
                KLOG_W("ERROR: Failed to read packet: %d", ret);
            }

            return ret;
        }
    }

    return OK;
}

/****************************************************************************
 * Name: psock_stream_recvfrom
 *
 * Description:
 *   psock_stream_recvfrom() receives messages from a local stream socket.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   buf      Buffer to receive data
 *   len      Length of buffer
 *   flags    Receive flags
 *   from     Address of source (may be NULL)
 *   fromlen  The length of the address structure
 *
 * Returned Value:
 *   On success, returns the number of characters received.  If no data is
 *   available to be received and the peer has performed an orderly shutdown,
 *   recv() will return 0.  Otherwise, on errors, -1 is returned, and errno
 *   is set appropriately (see receive from for the complete list).
 *
 ****************************************************************************/

ssize_t psock_stream_recvfrom(struct socket *psock, void *buf, size_t len, int flags,
                              struct sockaddr *from, socklen_t *fromlen)
{
    struct local_sock_s *lsock;
    size_t readlen = len;
    int ret;

    if (psock == NULL)
    {
        return -EINVAL;
    }
    lsock = psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }
    /* Verify that this is a connected peer socket */

    if (lsock->state != LOCAL_STATE_CONNECTED)
    {
        KLOG_W("ERROR:lsock rcv not connected");
        return -ENOTCONN;
    }

    /* Check shutdown state */

    if (lsock->infile.f_inode == NULL)
    {
        return 0;
    }

    /* If it is non-blocking mode, the data in fifo is 0 and
     * returns directly
     */

    if (flags & MSG_DONTWAIT)
    {
        int data_len = 0;
        ret = file_ioctl(&lsock->infile, FIONREAD, &data_len);
        if (ret < 0)
        {
            return ret;
        }

        if (data_len == 0)
        {
            return -EAGAIN;
        }
    }

    /* Read the packet */

    ret = psock_fifo_read(psock, buf, &readlen, flags, true);
    if (ret < 0)
    {
        return ret;
    }

    /* Return the address family */

    if (from)
    {
        ret = local_getaddr(lsock, from, fromlen);
        if (ret < 0)
        {
            return ret;
        }
    }
    return readlen;
}

/****************************************************************************
 * Name: psock_dgram_recvfrom
 *
 * Description:
 *   psock_dgram_recvfrom() receives messages from a local datagram socket.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   buf      Buffer to receive data
 *   len      Length of buffer
 *   flags    Receive flags
 *   from     Address of source (may be NULL)
 *   fromlen  The length of the address structure
 *
 * Returned Value:
 *   On success, returns the number of characters received.  Otherwise, on
 *   errors, -1 is returned, and errno is set appropriately (see receive
 *   from for the complete list).
 *
 ****************************************************************************/

ssize_t psock_dgram_recvfrom(struct socket *psock, void *buf, size_t len, int flags,
                             struct sockaddr *from, socklen_t *fromlen)
{
    struct local_sock_s *lsock;
    size_t readlen = 0;
    bool bclose = false;
    int ret = 0;

    if (psock == NULL)
    {
        return -EINVAL;
    }
    lsock = psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }
    /* Verify that this is a bound, un-connected peer socket */

    if (lsock->state != LOCAL_STATE_BOUND && lsock->state != LOCAL_STATE_CONNECTED)
    {
        /* Either not bound to address or it is connected */

        KLOG_W("ERROR: Connected or not bound");
        return -EISCONN;
    }

    if (lsock->infile.f_inode == NULL)
    {
        if (from != NULL)
            bclose = true;

        /* Make sure that half duplex FIFO has been created */

        ret = local_create_halfduplex(lsock, lsock->path);
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to create FIFO for %s: %d", lsock->path, ret);
            return ret;
        }

        /* Open the receiving side of the transfer */

        ret =
            local_open_receiver(lsock, (flags & MSG_DONTWAIT) != 0 || _SS_ISNONBLOCK(lsock->flags));
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to open FIFO for %s: %d", lsock->path, ret);
            goto errout_with_halfduplex;
        }
    }

    /* Sync to the start of the next packet in the stream and get the size of
     * the next packet.
     */

    if (lsock->pktlen <= 0)
    {
        ret = local_sync(&lsock->infile);
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to get packet length: %d", ret);
            goto errout_with_infd;
        }
        else if (ret > UINT16_MAX)
        {
            KLOG_W("ERROR: Packet is too big: %d", ret);
            goto errout_with_infd;
        }

        lsock->pktlen = (uint32_t)ret;
    }

    /* Read the packet */

    readlen = MIN(lsock->pktlen, len);
    ret = psock_fifo_read(psock, buf, &readlen, flags, false);
    if (ret < 0)
    {
        goto errout_with_infd;
    }

    /* If there are unread bytes remaining in the packet, flush the remainder
     * of the packet to the bit bucket.
     */

    if (flags & MSG_PEEK)
    {
        goto skip_flush;
    }

    if (readlen < lsock->pktlen)
    {
        uint8_t bitbucket[32];
        uint32_t remaining;
        size_t tmplen;

        remaining = lsock->pktlen - readlen;
        do
        {
            /* Read 32 bytes into the bit bucket */

            tmplen = MIN(remaining, 32);
            ret = psock_fifo_read(psock, bitbucket, &tmplen, flags, false);
            if (ret < 0)
            {
                goto errout_with_infd;
            }

            /* Adjust the number of bytes remaining to be read from the
             * packet
             */

            remaining -= tmplen;
        } while (remaining > 0);
    }

    /* The fifo has been read and the pktlen needs to be cleared */

    lsock->pktlen = 0;

skip_flush:

    /* Return the address family */

    if (from)
    {
        ret = local_getaddr(lsock, from, fromlen);
        if (ret < 0)
        {
            return ret;
        }
    }

errout_with_infd:

    /* Close the read-only file descriptor */

    if (bclose)
    {
        /* Now we can close the read-only file descriptor */

        file_close(&lsock->infile);
        lsock->infile.f_inode = NULL;

    errout_with_halfduplex:

        /* Release our reference to the half duplex FIFO */

        local_release_halfduplex(lsock);
    }

    return ret < 0 ? ret : readlen;
}
