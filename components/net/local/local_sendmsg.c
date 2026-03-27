/****************************************************************************
 * net/local/local_sendmsg.c
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
 * Name: local_send
 *
 * Description:
 *   Send a local packet as a stream.
 *
 * Input Parameters:
 *   psock    An instance of the internal socket structure.
 *   buf      Data to send
 *   len      Length of data to send
 *   flags    Send flags (ignored for now)
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error,
 *   -1 is returned, and errno is set appropriately (see send() for the
 *   list of errno numbers).
 *
 ****************************************************************************/

ssize_t local_send(struct socket *psock, const struct iovec *buf,
                   size_t len, int flags)
{
  ssize_t ret;

  if (psock == NULL)
  {
    return -EINVAL;
  }

  switch (psock->s_type)
    {
      case SOCK_STREAM:
        {
          struct local_sock_s *lsock = psock->s_priv;

          /* Verify that this is a connected peer socket and that it has
           * opened the outgoing FIFO for write-only access.
           */
          if (lsock->state != LOCAL_STATE_CONNECTED)
            {
              KLOG_W("ERROR:lsock snd not connected");
              return -ENOTCONN;
            }

          /* Check shutdown state */

          if (lsock->outfile.f_inode == NULL)
            {
              return -EPIPE;
            }

          /* Send the packet */
          ret = TTOS_ObtainMutex(lsock->sendlock, TTOS_SEMA_WAIT_FOREVER);
          if (ret < 0)
            {
              /* May fail because the task was canceled. */

              return ret;
            }

          ret = local_send_packet(&lsock->outfile, buf, len,
                                  psock->s_type == SOCK_DGRAM);
          TTOS_ReleaseMutex(lsock->sendlock);
        }
        break;
      case SOCK_DGRAM:
        {
          struct local_sock_s *lsock = psock->s_priv;

          /* Check shutdown state */

          if (lsock->outfile.f_inode == NULL)
          {
            return -EPIPE;
          }

          /* Send the packet */
          ret = TTOS_ObtainMutex(lsock->sendlock, TTOS_SEMA_WAIT_FOREVER);
          if (ret < 0)
          {
            /* May fail because the task was canceled. */
            return ret;
          }
          ret = local_send_packet(&lsock->outfile, buf, len,
                                  psock->s_type == SOCK_DGRAM);
          TTOS_ReleaseMutex(lsock->sendlock);
        }
        break;
      default:
        {
          /* EDESTADDRREQ.  Signifies that the socket is not connection-mode
           * and no peer address is set.
           */

          ret = -EDESTADDRREQ;
        }
        break;
    }

  return ret;
}

/****************************************************************************
 * Name: local_sendto
 *
 * Description:
 *   This function implements the Unix domain-specific logic of the
 *   standard sendto() socket operation.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   buf      Data to send
 *   len      Length of data to send
 *   flags    Send flags
 *   to       Address of recipient
 *   tolen    The length of the address structure
 *
 *   NOTE: All input parameters were verified by sendto() before this
 *   function was called.
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error,
 *   a negated errno value is returned.  See the description in
 *   net/socket/sendto.c for the list of appropriate return value.
 *
 ****************************************************************************/

ssize_t local_sendto(struct socket *psock, const struct iovec *buf,
                     size_t len, int flags, const struct sockaddr *to,
                     socklen_t tolen)
{
  struct local_sock_s *lsock;
  struct sockaddr_un *unaddr = (struct sockaddr_un *)to;
  ssize_t ret;

  if (psock == NULL || to == NULL)
  {
    return -EINVAL;
  }
  lsock = psock->s_priv;
  if (lsock == NULL)
  {
    return -EINVAL;
  }
  /* Verify that a valid address has been provided */

  if (to->sa_family != AF_LOCAL || tolen < sizeof(sa_family_t))
    {
      KLOG_W("ERROR: Unrecognized address family: %d",
           to->sa_family);
      return -EAFNOSUPPORT;
    }

  /* If this is a connected socket, then return EISCONN */

  if (psock->s_type != SOCK_DGRAM)
    {
      KLOG_W("ERROR: Connected socket");
      return -EISCONN;
    }

  /* Verify that this is not a connected peer socket.  It need not be
   * bound, however.  If unbound, recvfrom will see this as a nameless
   * connection.
   */

  if (lsock->state != LOCAL_STATE_UNBOUND &&
      lsock->state != LOCAL_STATE_BOUND)
    {
      /* Either not bound to address or it is connected */

      KLOG_W("ERROR: Connected state");
      return -EISCONN;
    }

  /* At present, only standard pathname type address are support */

  if (tolen < sizeof(sa_family_t) + 2)
    {
      /* EFAULT
       * - An invalid user space address was specified for a parameter
       */

      return -EFAULT;
    }

  /* Make sure that half duplex FIFO has been created.
   * REVISIT:  Or should be just make sure that it already exists?
   */

  ret = local_create_halfduplex(lsock, unaddr->sun_path);
  if (ret < 0)
    {
      KLOG_W("ERROR: Failed to create FIFO for %s: %" PRIdPTR,
           lsock->path, ret);
      return ret;
    }

  /* Open the sending side of the transfer */

  ret = local_open_sender(lsock, unaddr->sun_path,
                          _SS_ISNONBLOCK(lsock->flags) ||
                          (flags & MSG_DONTWAIT) != 0);
  if (ret < 0)
    {
      KLOG_W("ERROR: Failed to open FIFO for %s: %" PRIdPTR,
           unaddr->sun_path, ret);

      goto errout_with_halfduplex;
    }

  /* Make sure that dgram is sent safely */

  ret = TTOS_ObtainMutex(lsock->sendlock, TTOS_SEMA_WAIT_FOREVER);
  if (ret < 0)
    {
      /* May fail because the task was canceled. */

      goto errout_with_sender;
    }

  /* Send the packet */
  ret = local_send_packet(&lsock->outfile, buf, len, true);
  if (ret < 0)
    {
      KLOG_W("ERROR: Failed to send the packet: %" PRIdPTR, ret);
    }

  TTOS_ReleaseMutex(lsock->sendlock);

errout_with_sender:

  /* Now we can close the write-only socket descriptor */

  file_close(&lsock->outfile);
  lsock->outfile.f_inode = NULL;

errout_with_halfduplex:

  /* Release our reference to the half duplex FIFO */

  local_release_halfduplex(lsock);
  return ret;
}
