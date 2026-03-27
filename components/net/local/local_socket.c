/****************************************************************************
 * net/local/local_release.c
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

/****************************************************************************
 * Name: local_generate_instance_id
 *
 * Description:
 *   Generate instance ID for stream
 *
 ****************************************************************************/

uint32_t local_generate_instance_id(void)
{
    static uint32_t g_next_instance_id = 0;
    int32_t id;

    /* Called from local_connect with net_lock held. */

    id = g_next_instance_id++;

    return id;
}

/****************************************************************************
 * Name: local_alloc()
 *
 * Description:
 *   Allocate a new, uninitialized Unix domain socket connection structure.
 *   This is normally something done by the implementation of the socket()
 *   API
 *
 ****************************************************************************/

struct local_sock_s *local_alloc(void)
{
    struct local_sock_s *lsock = malloc(sizeof(struct local_sock_s));
    irq_flags_t glocal_flags;

    if (lsock != NULL)
    {
        /* Initialize non-zero elements the new connection structure.  Since
         * Since the memory was allocated with kmm_zalloc(), it is not
         * necessary to zerio-ize any structure elements.
         */

        TTOS_CreateSemaEx(0, &lsock->waitsem);
        /* This semaphore is used for sending safely in multithread.
         * Make sure data will not be garbled when multi-thread sends.
         */

        TTOS_CreateMutex(1, 0, &lsock->sendlock);
        TTOS_CreateMutex(1, 0, &lsock->polllock);

        /* Add the connection structure to the list of listeners */

        spin_lock_irqsave(&g_local_conn_lock, glocal_flags);
        list_add_tail(&lsock->node, &g_local_connections);
        spin_unlock_irqrestore(&g_local_conn_lock, glocal_flags);
    }

    return lsock;
}

/****************************************************************************
 * Name: local_free()
 *
 * Description:
 *   Free a packet Unix domain connection structure that is no longer in use.
 *   This should be done by the implementation of close().
 *
 ****************************************************************************/

void local_free(struct local_sock_s *lsock)
{
    irq_flags_t glocal_flags;
    irq_flags_t wait_flags;

    /* Remove the server from the list of listeners. */

    spin_lock_irqsave(&g_local_conn_lock, glocal_flags);
    list_delete(&lsock->node);
    spin_unlock_irqrestore(&g_local_conn_lock, glocal_flags);

    if (local_peerconn(lsock) && lsock->peer)
    {
        lsock->peer->peer = NULL;
        lsock->peer = NULL;
    }

    /* Make sure that the read-only FIFO is closed */

    if (lsock->infile.f_inode != NULL)
    {
        file_close(&lsock->infile);
        lsock->infile.f_inode = NULL;
    }

    /* Make sure that the write-only FIFO is closed */

    if (lsock->outfile.f_inode != NULL)
    {
        file_close(&lsock->outfile);
        lsock->outfile.f_inode = NULL;
    }

    /* Destroy all FIFOs associted with the connection */

    local_release_fifos(lsock);

    TTOS_DeleteSema(lsock->waitsem);

    /* Destory sem associated with the connection */

    TTOS_DeleteMutex(lsock->sendlock);
    TTOS_DeleteMutex(lsock->polllock);

    /* And free the connection structure */

    free(lsock);
}

/****************************************************************************
 * Name: local_release
 *
 * Description:
 *   If the local, Unix domain socket is in the connected state, then
 *   disconnect it.  Release the local connection structure in any event
 *
 * Input Parameters:
 *   lsock - A reference to local connection structure
 *
 ****************************************************************************/

int local_release(struct local_sock_s *lsock)
{
    irq_flags_t wait_flags;
    struct local_sock_s *client;
    struct local_sock_s *__tmp;

    /* We should not bet here with state LOCAL_STATE_ACCEPT.  That is an
     * internal state that should be atomic with respect to socket operations.
     */

    assert(lsock->state != LOCAL_STATE_ACCEPT);

    /* Is the socket is listening socket (SOCK_STREAM server) */

    if (lsock->state == LOCAL_STATE_LISTENING)
    {
        /* Are there still clients waiting for a connection to the server? */

        spin_lock_irqsave(&lsock->server_waiters_lock, wait_flags);
        list_for_each_entry_safe(client, __tmp, &lsock->u.server.waiters, u.accept.waiter)
        {
            list_delete(&client->u.accept.waiter);
            client->state = LOCAL_STATE_DISCONNECTED;
            TTOS_ReleaseSema(client->waitsem);
        }
        spin_unlock_irqrestore(&lsock->server_waiters_lock, wait_flags);

        lsock->u.server.pending = 0;
    }

    /* For the remaining states (LOCAL_STATE_UNBOUND and LOCAL_STATE_UNBOUND),
     * we simply free the connection structure.
     */

    /* Free the connection structure */

    local_free(lsock);
    return OK;
}
