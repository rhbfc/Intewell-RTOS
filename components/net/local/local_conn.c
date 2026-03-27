/****************************************************************************
 * net/local/local_conn.c
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
#include <restart.h>

#define KLOG_TAG "AF_LOCAL"
#include <klog.h>

/****************************************************************************
 * Name: local_alloc_accept
 *
 * Description:
 *    Called when a client calls connect and can find the appropriate
 *    connection in LISTEN. In that case, this function will create
 *    a new connection and initialize it.
 *
 ****************************************************************************/

int local_alloc_accept(struct local_sock_s *server, struct local_sock_s *client,
                       struct local_sock_s **accept)
{
    struct local_sock_s *lsock;

    /* Create a new connection structure for the server side of the
     * connection.
     */

    lsock = local_alloc();
    if (lsock == NULL)
    {
        KLOG_W("ERROR:  Failed to allocate new connection structure");
        return -ENOMEM;
    }

    lsock->proto = SOCK_STREAM;
    lsock->type = LOCAL_TYPE_PATHNAME;
    lsock->state = LOCAL_STATE_UNBOUND;
    lsock->peer = client;
    client->peer = lsock;

    strlcpy(lsock->path, client->path, sizeof(lsock->path));
    lsock->instance_id = server->instance_id;
    client->instance_id = server->instance_id;

    // /* Open the server-side write-only FIFO.  This should not
    //  * block.
    //  */

    // ret = local_open_server_tx(lsock, false);
    // if (ret < 0)
    //   {
    //     KLOG_W("ERROR: Failed to open write-only FIFOs for %s: %d",
    //          lsock->path, ret);
    //     goto err;
    //   }

    // /* Do we have a connection?  Is the write-side FIFO opened? */

    // if(lsock->outfile.f_inode == NULL)return EINVAL;

    // /* Open the server-side read-only FIFO.  This should not
    //  * block because the client side has already opening it
    //  * for writing.
    //  */

    // ret = local_open_server_rx(lsock, false);
    // if (ret < 0)
    //   {
    //     KLOG_W("ERROR: Failed to open read-only FIFOs for %s: %d",
    //          lsock->path, ret);
    //     goto err;
    //   }

    // /* Do we have a connection?  Are the FIFOs opened? */

    // if(lsock->infile.f_inode == NULL)return EINVAL;
    *accept = lsock;
    return OK;

    // err:
    // local_free(lsock);
    // return ret;
}

/****************************************************************************
 * Name: local_stream_connect
 *
 * Description:
 *   Find a local connection structure that is the appropriate "server"
 *   connection to be used with the provided "client" connection.
 *
 * Returned Value:
 *   Zero (OK) returned on success; A negated errno value is returned on a
 *   failure.  Possible failures include:
 *
 * Assumptions:
 *   The network is locked on entry, unlocked on return.  This logic is
 *   an integral part of the lock_connect() implementation and was
 *   separated out only to improve readability.
 *
 ****************************************************************************/

int local_stream_connect(struct local_sock_s *client, struct local_sock_s *server, bool nonblock)
{
    irq_flags_t wait_flags;
    int ret;

    /* Has server backlog been reached?
     * NOTE: The backlog will be zero if listen() has never been called by the
     * server.
     */

    if (server->state != LOCAL_STATE_LISTENING ||
        server->u.server.pending >= server->u.server.backlog)
    {
        KLOG_W("ERROR: Server is not listening: lc_state=%d", server->state);
        return -ECONNREFUSED;
    }

    server->u.server.pending++;

    /* Add ourself to the list of waiting connections and notify the server. */
    spin_lock_irqsave(&server->server_waiters_lock, wait_flags);
    list_add_tail(&client->u.accept.waiter, &server->u.server.waiters);
    spin_unlock_irqrestore(&server->server_waiters_lock, wait_flags);

    TTOS_ReleaseSema(server->waitsem);

    /* Wait for server to create the FIFOs */
    ret = TTOS_ObtainSema(client->waitsem, TTOS_WAIT_FOREVER);
    if (TTOS_OK != ret)
    {
        if (TTOS_SIGNAL_INTR == ret)
        {
            ret = -ERR_RESTART_IF_SIGNAL;
        }
        else
        {
            ret = -ret;
        }
        goto errout_with_conn;
    }

    if (LOCAL_STATE_DISCONNECTED == client->state)
    {
        ret = -ECONNREFUSED;
        goto errout_with_conn;
    }

    client->state = LOCAL_STATE_BOUND;

    /* Write only side */

    ret = local_open_client_tx(client, nonblock);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to open write-only FIFOs for %s: %d", client->path, ret);
        goto errout_with_fifos;
    }

    /* Read only side */

    ret = local_open_client_rx(client, nonblock);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to open read-only FIFOs for %s: %d", client->path, ret);
        goto errout_with_conn;
    }

    client->state = LOCAL_STATE_CONNECTED;

    return OK;

errout_with_conn:
errout_with_outfd:
    file_close(&client->outfile);
    client->outfile.f_inode = NULL;

errout_with_fifos:
    local_release_fifos(client);
    client->state = LOCAL_STATE_UNBOUND;
    return ret;
}

/****************************************************************************
 * Name: local_peerconn
 *
 * Description:
 *   Traverse the connections list to find the peer
 *
 * Assumptions:
 *   This function must be called with the network locked.
 *
 ****************************************************************************/

struct local_sock_s *local_peerconn(struct local_sock_s *lsock)
{
    struct local_sock_s *peer = NULL;

    if (lsock == NULL)
    {
        return NULL;
    }
    while ((peer = local_nextconn(peer)) != NULL)
    {
        if (lsock->proto == peer->proto && lsock != peer &&
            !strncmp(lsock->path, peer->path, UNIX_PATH_MAX - 1))
        {
            return peer;
        }
    }

    return NULL;
}

/****************************************************************************
 * Name: local_addref
 *
 * Description:
 *   Increment the reference count on the underlying connection structure.
 *
 * Input Parameters:
 *   psock - Socket structure of the socket whose reference count will be
 *           incremented.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void local_addref(struct socket *psock) {}
