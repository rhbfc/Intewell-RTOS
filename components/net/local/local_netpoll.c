/****************************************************************************
 * net/local/local_netpoll.c
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
#include <ttosBase.h>

static int local_event_pollsetup(struct local_sock_s *lsock,
                                 struct kpollfd *fds,
                                 bool setup)
{
    int i;


  if (lsock == NULL || fds == NULL)
  {
    return -EINVAL;
  }
  if (setup)
  {
    /* This is a request to set up the poll.  Find an available
    * slot for the poll structure reference
    */

    TTOS_ObtainMutex(lsock->polllock, TTOS_SEMA_WAIT_FOREVER);
    ttosDisableTaskDispatchWithLock();

    for (i = 0; i < LOCAL_NPOLLWAITERS; i++)
    {
      /* Find an available slot */

      if (!lsock->event_fds[i])
      {
        /* Bind the poll structure and this slot */

        lsock->event_fds[i] = fds;
        fds->priv = &lsock->event_fds[i];
        break;
      }
    }

    if (i >= LOCAL_NPOLLWAITERS)
    {
      fds->priv = NULL;
      return -EBUSY;
    }

    if (lsock->state == LOCAL_STATE_LISTENING)//&conn->u.server.lc_waiters != NULL
    {
      kpoll_notify(&fds, 1, POLLIN, NULL);
    }
  }
  else
  {

    *(struct kpollfd **)fds->priv = NULL;
    fds->priv = NULL;

    
  }

  ttosEnableTaskDispatchWithLock();
  TTOS_ReleaseMutex(lsock->polllock);

  return OK;
}

static void local_inout_poll_cb(struct kpollfd *fds)
{
  struct kpollfd *originfds;

  if (fds == NULL)
  {
    return;
  }
  originfds = fds->arg;
  kpoll_notify(&originfds, 1, fds->pollfd.revents, NULL);
}

void local_event_pollnotify(struct local_sock_s *lsock,
                            uint32_t eventset)
{
  if (lsock == NULL)
  {
    return;
  }
  TTOS_ObtainMutex(lsock->polllock, TTOS_SEMA_WAIT_FOREVER);
  kpoll_notify(lsock->event_fds, LOCAL_NPOLLWAITERS, eventset, NULL);
  TTOS_ReleaseMutex(lsock->polllock);
}

/****************************************************************************
 * Name: local_pollsetup
 *
 * Description:
 *   Setup to monitor events on one Unix domain socket
 *
 * Input Parameters:
 *   psock - The Unix domain socket of interest
 *   fds   - The structure describing the events to be monitored, OR NULL if
 *           this is a request to stop monitoring events.
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/

int local_pollsetup(struct socket *psock, struct kpollfd *fds)
{
  struct local_sock_s *lsock;
  int ret = OK;

  if (psock == NULL || fds == NULL)
  {
    return -EINVAL;
  }
  lsock = psock->s_priv;
  if (lsock == NULL)
  {
    return -EINVAL;
  }
  if (lsock->state == LOCAL_STATE_LISTENING)
  {
    return local_event_pollsetup(lsock, fds, true);
  }

  if (lsock->state == LOCAL_STATE_DISCONNECTED)
  {
    fds->priv = NULL;
    goto pollerr;
  }

  switch (fds->pollfd.events & (POLLIN | POLLOUT))
  {
    case (POLLIN | POLLOUT):
      {
        struct kpollfd *shadowfds;

        /* Poll wants to check state for both input and output. */

        if (lsock->infile.f_inode == NULL ||
            lsock->outfile.f_inode == NULL)
        {
          fds->priv = NULL;
          goto pollerr;
        }

        /* Find shadow pollfds. */

        TTOS_ObtainMutex(lsock->polllock, TTOS_SEMA_WAIT_FOREVER);

        shadowfds = lsock->inout_fds;
        while (shadowfds->pollfd.fd != 0)
        {
          shadowfds += 2;
          if (shadowfds >= &lsock->inout_fds[2*LOCAL_NPOLLWAITERS])
          {
            TTOS_ReleaseMutex(lsock->polllock);
            return -ENOMEM;
          }
        }

        shadowfds[0]         = *fds;
        shadowfds[0].pollfd.fd      = 1; /* Does not matter */
        shadowfds[0].cb      = local_inout_poll_cb;
        shadowfds[0].arg     = fds;
        shadowfds[0].pollfd.events &= ~POLLOUT;

        shadowfds[1]         = *fds;
        shadowfds[1].pollfd.fd      = 0; /* Does not matter */
        shadowfds[1].cb      = local_inout_poll_cb;
        shadowfds[1].arg     = fds;
        shadowfds[1].pollfd.events &= ~POLLIN;

        TTOS_ReleaseMutex(lsock->polllock);

        /* Setup poll for both shadow pollfds. */

        ret = file_poll(&lsock->infile, &shadowfds[0], true);
        if (ret >= 0)
        {
          ret = file_poll(&lsock->outfile, &shadowfds[1], true);
          if (ret < 0)
          {
            file_poll(&lsock->infile, &shadowfds[0], false);
          }
        }

        if (ret < 0)
        {
          shadowfds[0].pollfd.fd = 0;
          fds->priv = NULL;
          goto pollerr;
        }
        else
        {
          fds->priv = shadowfds;
        }
      }
      break;

    case POLLIN:
      {
        /* Poll wants to check state for input only. */

        if (lsock->infile.f_inode == NULL)
        {
          fds->priv = NULL;
          goto pollerr;
        }

        ret = file_poll(&lsock->infile, fds, true);
      }
      break;

    case POLLOUT:
      {
        /* Poll wants to check state for output only. */

        if (lsock->outfile.f_inode == NULL)
        {
          fds->priv = NULL;
          goto pollerr;
        }

        ret = file_poll(&lsock->outfile, fds, true);
      }
      break;

    default:
      break;
  }

  return ret;

pollerr:
  kpoll_notify(&fds, 1, POLLERR, NULL);
  return OK;
}

/****************************************************************************
 * Name: local_pollteardown
 *
 * Description:
 *   Teardown monitoring of events on a Unix domain socket
 *
 * Input Parameters:
 *   psock - The Unix domain socket of interest
 *   fds   - The structure describing the events to be monitored, OR NULL if
 *           this is a request to stop monitoring events.
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/

int local_pollteardown(struct socket *psock, struct kpollfd *fds)
{
  struct local_sock_s *lsock;
  int ret = OK;

  if (psock == NULL || fds == NULL)
  {
    return -EINVAL;
  }
  lsock = psock->s_priv;
  if (lsock == NULL)
  {
    return -EINVAL;
  }
  if (lsock->state == LOCAL_STATE_LISTENING)
    {
      return local_event_pollsetup(lsock, fds, false);
    }

  if (lsock->state == LOCAL_STATE_DISCONNECTED)
    {
      return OK;
    }

  switch (fds->pollfd.events & (POLLIN | POLLOUT))
    {
      case (POLLIN | POLLOUT):
        {
          struct kpollfd *shadowfds = fds->priv;
          int ret2;

          if (shadowfds == NULL)
            {
              return OK;
            }

          /* Teardown for both shadow pollfds. */

          ret = file_poll(&lsock->infile, &shadowfds[0], false);
          ret2 = file_poll(&lsock->outfile, &shadowfds[1], false);
          if (ret2 < 0)
            {
              ret = ret2;
            }

          fds->pollfd.revents |= shadowfds[0].pollfd.revents | shadowfds[1].pollfd.revents;
          fds->priv = NULL;
          shadowfds[0].pollfd.fd = 0;
        }
        break;

      case POLLIN:
        {
          if (fds->priv == NULL)
            {
              return OK;
            }

          ret = file_poll(&lsock->infile, fds, false);
        }
        break;

      case POLLOUT:
        {
          if (fds->priv == NULL)
            {
              return OK;
            }

          ret = file_poll(&lsock->outfile, fds, false);
        }
        break;

      default:
        break;
    }

  return ret;
}
