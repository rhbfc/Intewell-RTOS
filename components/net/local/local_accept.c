/****************************************************************************
 * net/local/local_accept.c
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
#include <restart.h>

/****************************************************************************
 * Name: local_accept
 *
 * Description:
 *   This function implements accept() for Unix domain sockets.  See the
 *   description of accept() for further information.
 *
 * Input Parameters:
 *   psock    The listening Unix domain socket structure
 *   addr     Receives the address of the connecting client
 *   addrlen  Input: allocated size of 'addr',
 *            Return: returned size of 'addr'
 *   newconn  The new, accepted  Unix domain connection structure
 *
 * Returned Value:
 *   Returns zero (OK) on success or a negated errno value on failure.
 *   See the description of accept of the possible errno values in the
 *   description of accept().
 *
 * Assumptions:
 *   Network is NOT locked.
 *
 ****************************************************************************/

int local_accept(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen,
                 struct socket *newsock, int flags)
{
    struct local_sock_s *server = psock->s_priv;
    struct local_sock_s *client;
    struct local_sock_s *newconn;
    struct list_node *waiter;
    irq_flags_t wait_flags;
    bool nonblock = _SS_ISNONBLOCK(flags);
    int ret = 0;

    /* Is the socket a stream? */

    if (psock->s_domain != PF_LOCAL || psock->s_type != SOCK_STREAM)
    {
        return -EOPNOTSUPP;
    }
    if (server->proto != SOCK_STREAM || server->state != LOCAL_STATE_LISTENING)
    {
        return -EOPNOTSUPP;
    }

    if (nonblock)
    {
        ret = TTOS_ObtainSema(server->waitsem, 0);
    }
    else
    {
        ret = TTOS_ObtainSema(server->waitsem, TTOS_SEMA_WAIT_FOREVER);
    }

    if (TTOS_SIGNAL_INTR == ret)
    {
        return -ERR_RESTART_IF_SIGNAL;
    }
    else if (nonblock && TTOS_UNSATISFIED == ret)
    {
        return -EAGAIN;
    }
    else if (ret != TTOS_OK)
    {
        return -ret;
    }

    spin_lock_irqsave(&server->server_waiters_lock, wait_flags);
    waiter = list_delete_head(&server->u.server.waiters);
    spin_unlock_irqrestore(&server->server_waiters_lock, wait_flags);

    assert(!!waiter);

    client = container_of(waiter, struct local_sock_s, u.accept.waiter);

    if ((ret = local_create_fifos(server)) != OK)
    {
        goto err_accept_newconn;
    }

    /* lsock server拿到的client端口 */
    ret = local_alloc_accept(server, client, &newconn);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to alloc accept conn %s: %d", client->path, ret);
        goto err_accept_newconn;
    }

    /* Decrement the number of pending accpets */

    server->u.server.pending--;
    newconn->instance_id = server->instance_id;

    /* Setup the accpet socket structure */

    newsock->s_domain = psock->s_domain;
    newsock->s_type = SOCK_STREAM;
    newsock->s_sockif = psock->s_sockif;
    newsock->s_priv = (void *)newconn;

    /* Return the address family */

    if (addr != NULL)
    {
        ret = local_getaddr(newconn, addr, addrlen);
    }

    if (ret == OK && nonblock)
    {
        ret = local_set_nonblocking(newconn);
        if (ret < 0)
        {
            goto err_accept_newconn;
        }
    }
    else if (ret < 0)
    {
        goto err_accept_newconn;
    }

    newconn->state = LOCAL_STATE_BOUND;

    /* notify connection is ready */
    TTOS_ReleaseSema(client->waitsem);

    /* Open the server-side read-only FIFO.  This should not
     * block because the client side has already opening it
     * for writing.
     */

    ret = local_open_server_rx(newconn, false);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to open read-only FIFOs for %s: %d", newconn->path, ret);
        // local_free(lsock);
        return ret;
    }

    /* Do we have a connection?  Are the FIFOs opened? */

    if (newconn->infile.f_inode == NULL)
    {
        return -EINVAL;
    }

    /* Open the server-side write-only FIFO.  This should not
     * block.
     */

    ret = local_open_server_tx(newconn, false);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to open write-only FIFOs for %s: %d", newconn->path, ret);
        // local_free(lsock);
        return ret;
    }

    /* Do we have a connection?  Is the write-side FIFO opened? */

    if (newconn->outfile.f_inode == NULL)
    {
        return -EINVAL;
    }

    newconn->state = LOCAL_STATE_CONNECTED;

    return OK;

err_accept_newconn:
    client->state = LOCAL_STATE_DISCONNECTED;
    TTOS_ReleaseSema(client->waitsem);
    return ret;
}
