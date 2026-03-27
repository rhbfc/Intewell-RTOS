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
#include <net/if.h>
#include <net/netdev_ioctl.h>
#include <ttos_init.h>
/* PUBLIC */

#define KLOG_TAG "AF_LOCAL"
#include <klog.h>

struct list_node g_local_connections;
DEFINE_SPINLOCK(g_local_conn_lock);

/****************************************************************************
 * Name: local_nextconn
 *
 * Description:
 *   Traverse the list of local connections
 *
 * Assumptions:
 *   This function must be called with the network locked.
 *
 ****************************************************************************/

struct local_sock_s *local_nextconn(struct local_sock_s *lsock)
{
    irq_flags_t glocal_flags;

    struct local_sock_s *nextconn = NULL;

    spin_lock_irqsave(&g_local_conn_lock, glocal_flags);

    if (NULL == lsock)
    {
        if (g_local_connections.next != &g_local_connections)
        {
            nextconn = container_of(g_local_connections.next, struct local_sock_s, node);
        }
    }
    else if (lsock->node.next && lsock->node.next != &lsock->node &&
             lsock->node.next != &g_local_connections)
    {
        nextconn = container_of(lsock->node.next, struct local_sock_s, node);
    }

    spin_unlock_irqrestore(&g_local_conn_lock, glocal_flags);

    return nextconn;
}

/****************************************************************************
 * Name: local_setup
 *
 * Description:
 *   Called for socket() to verify that the provided socket type and
 *   protocol are usable by this address family.  Perform any family-
 *   specific socket fields.
 *
 * Input Parameters:
 *   psock    A pointer to a user allocated socket structure
 *            to be initialized.
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  Otherwise, a negated errno value is
 *   returned.
 *
 ****************************************************************************/

static int local_setup(struct socket *psock)
{
    struct local_sock_s *lsock;
    irq_flags_t glocal_flags;

    if (psock == NULL)
    {
        return -EINVAL;
    }
    lsock = calloc(1, sizeof(struct local_sock_s));
    if (lsock == NULL)
    {
        /* Failed to reserve a connection structure */
        return -ENOMEM;
    }

    TTOS_CreateSemaEx(0, &lsock->waitsem);
    TTOS_CreateMutex(1, 0, &lsock->sendlock);
    TTOS_CreateMutex(1, 0, &lsock->polllock);
    spin_lock_irqsave(&g_local_conn_lock, glocal_flags);
    list_add_tail(&lsock->node, &g_local_connections);
    spin_unlock_irqrestore(&g_local_conn_lock, glocal_flags);
    INIT_LIST_HEAD(&lsock->u.server.waiters);
    spin_lock_init(&lsock->server_waiters_lock);

    psock->s_priv = lsock;
    return OK;
}

/****************************************************************************
 * Name: local_sockcaps
 *
 * Description:
 *   Return the bit encoded capabilities of this socket.
 *
 * Input Parameters:
 *   psock - Socket structure of the socket whose capabilities are being
 *           queried.
 *
 * Returned Value:
 *   The set of socket cababilities is returned.
 *
 ****************************************************************************/

static sockcaps_t local_sockcaps(struct socket *psock)
{
    return SOCKCAP_NONBLOCKING;
}

/****************************************************************************
 * Name: local_bind
 *
 * Description:
 *   local_bind() gives the socket 'psock' the local address 'addr'.  'addr'
 *   is 'addrlen' bytes long.  Traditionally, this is called "assigning a
 *   name to a socket."  When a socket is created with socket(), it exists
 *   in a name space (address family) but has no name assigned.
 *
 * Input Parameters:
 *   psock    Socket structure of the socket to bind
 *   addr     Socket local address
 *   addrlen  Length of 'addr'
 *
 * Returned Value:
 *   0 on success;  A negated errno value is returned on failure.  See
 *   bind() for a list a appropriate error values.
 *
 ****************************************************************************/

static int local_bind(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    struct local_sock_s *lsock;
    const struct sockaddr_un *unaddr = (const struct sockaddr_un *)addr;
    int ret;

    if (psock == NULL || addr == NULL)
    {
        return -EINVAL;
    }
    lsock = (struct local_sock_s *)psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }

    if (strnlen(unaddr->sun_path, LOCAL_FULLPATH_LEN) == LOCAL_FULLPATH_LEN)
    {
        return -ENAMETOOLONG;
    }

    lsock->proto = psock->s_type;
    if (unaddr->sun_path[0] == '\0')
    {
        /* Zero-length sun_path... This is an abstract Unix domain socket */

        lsock->type = LOCAL_TYPE_ABSTRACT;

        /* Copy the path into the connection structure */

        strlcpy(lsock->path, &unaddr->sun_path[1], sizeof(lsock->path) - 1);
    }
    else
    {
        /* This is an normal, pathname Unix domain socket */

        lsock->type = LOCAL_TYPE_PATHNAME;

        /* Copy the path into the connection structure */

        strlcpy(lsock->path, unaddr->sun_path, sizeof(lsock->path));
    }

    lsock->state = LOCAL_STATE_BOUND;

    return OK;
}

/****************************************************************************
 * Name: local_getsockname
 *
 * Description:
 *   The local_getsockname() function retrieves the locally-bound name of
 *   the specified local socket, stores this address in the sockaddr
 *   structure pointed to by the 'addr' argument, and stores the length of
 *   this address in the object pointed to by the 'addrlen' argument.
 *
 *   If the actual length of the address is greater than the length of the
 *   supplied sockaddr structure, the stored address will be truncated.
 *
 *   If the socket has not been bound to a local name, the value stored in
 *   the object pointed to by address is unspecified.
 *
 * Input Parameters:
 *   psock    Socket structure of the socket to be queried
 *   addr     sockaddr structure to receive data [out]
 *   addrlen  Length of sockaddr structure [in/out]
 *
 * Returned Value:
 *   On success, 0 is returned, the 'addr' argument points to the address
 *   of the socket, and the 'addrlen' argument points to the length of the
 *   address.  Otherwise, a negated errno value is returned.  See
 *   getsockname() for the list of appropriate error numbers.
 *
 ****************************************************************************/

static int local_getsockname(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    struct local_sock_s *lsock;
    struct sockaddr_un *unaddr = (struct sockaddr_un *)addr;

    if (psock == NULL || addr == NULL)
    {
        return -EINVAL;
    }
    lsock = (struct local_sock_s *)psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }

    unaddr->sun_family = AF_LOCAL;

    if (lsock->type == LOCAL_TYPE_UNNAMED)
    {
        *addrlen = sizeof(sa_family_t);
    }
    else
    {
        size_t namelen = strlen(lsock->path) + 1 + (lsock->type == LOCAL_TYPE_ABSTRACT);
        size_t pathlen = *addrlen - sizeof(sa_family_t);
        if (pathlen < namelen)
        {
            namelen = pathlen;
        }
        if (lsock->type == LOCAL_TYPE_ABSTRACT)
        {
            unaddr->sun_path[0] = '\0';
            strlcpy(&unaddr->sun_path[1], lsock->path, namelen - 1);
        }
        else
        {
            strlcpy(unaddr->sun_path, lsock->path, namelen);
        }

        *addrlen = sizeof(sa_family_t) + namelen;
    }

    return OK;
}

/****************************************************************************
 * Name: local_getpeername
 *
 * Description:
 *   The local_getpeername() function retrieves the remote-connected name of
 *   the specified local socket, stores this address in the sockaddr
 *   structure pointed to by the 'addr' argument, and stores the length of
 *   this address in the object pointed to by the 'addrlen' argument.
 *
 *   If the actual length of the address is greater than the length of the
 *   supplied sockaddr structure, the stored address will be truncated.
 *
 *   If the socket has not been bound to a local name, the value stored in
 *   the object pointed to by address is unspecified.
 *
 * Parameters:
 *   psock    Socket structure of the socket to be queried
 *   addr     sockaddr structure to receive data [out]
 *   addrlen  Length of sockaddr structure [in/out]
 *
 * Returned Value:
 *   On success, 0 is returned, the 'addr' argument points to the address
 *   of the socket, and the 'addrlen' argument points to the length of the
 *   address.  Otherwise, a negated errno value is returned.  See
 *   getpeername() for the list of appropriate error numbers.
 *
 ****************************************************************************/

static int local_getpeername(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    struct local_sock_s *lsock;
    struct local_sock_s *lpeer;
    struct sockaddr_un *unaddr = (struct sockaddr_un *)addr;

    if (psock == NULL || addr == NULL)
    {
        return -EINVAL;
    }
    lsock = (struct local_sock_s *)psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }
    lpeer = lsock->peer;
    if (lpeer == NULL)
    {
        return -EINVAL;
    }

    if (*addrlen < sizeof(sa_family_t))
    {
        /* This is apparently not an error */

        *addrlen = 0;
        return OK;
    }
    if (lsock->state != LOCAL_STATE_CONNECTED)
    {
        return -ENOTCONN;
    }

    unaddr->sun_family = AF_LOCAL;
    if (lpeer->type == LOCAL_TYPE_UNNAMED)
    {
        *addrlen = sizeof(sa_family_t);
    }
    else
    {
        size_t namelen = strlen(lpeer->path) + 1 + (lpeer->type == LOCAL_TYPE_ABSTRACT);
        if (lpeer->type == LOCAL_TYPE_ABSTRACT)
        {
            unaddr->sun_path[0] = '\0';
            strlcpy(&unaddr->sun_path[1], lpeer->path, namelen - 1);
        }
        else
        {
            strlcpy(unaddr->sun_path, lpeer->path, namelen);
        }

        *addrlen = sizeof(sa_family_t) + namelen;
    }

    return OK;
}

/****************************************************************************
 * Name: local_listen
 *
 * Description:
 *   To accept connections, a socket is first created with psock_socket(), a
 *   willingness to accept incoming connections and a queue limit for
 *   incoming connections are specified with psock_listen(), and then the
 *   connections are accepted with psock_accept().  For the case of local
 *   Unix sockets, psock_listen() calls this function.  The psock_listen()
 *   call applies only to sockets of type SOCK_STREAM or SOCK_SEQPACKET.
 *
 * Input Parameters:
 *   psock    Reference to an internal, boound socket structure.
 *   backlog  The maximum length the queue of pending connections may grow.
 *            If a connection request arrives with the queue full, the client
 *            may receive an error with an indication of ECONNREFUSED or,
 *            if the underlying protocol supports retransmission, the request
 *            may be ignored so that retries succeed.
 *
 * Returned Value:
 *   On success, zero is returned. On error, a negated errno value is
 *   returned.  See listen() for the set of appropriate error values.
 *
 ****************************************************************************/

int local_listen(struct socket *psock, int backlog)
{
    struct local_sock_s *server;
    int ret;

    if (psock == NULL)
    {
        return -EINVAL;
    }

    server = (struct local_sock_s *)psock->s_priv;

    if (server == NULL)
    {
        return -EINVAL;
    }

    if (psock->s_domain != PF_LOCAL || psock->s_type != SOCK_STREAM)
    {
        KLOG_W("ERROR: Unsupported socket family=%d or socket type=%d", psock->s_domain,
               psock->s_type);
        return -EOPNOTSUPP;
    }

    if (server->proto != SOCK_STREAM || server->state == LOCAL_STATE_UNBOUND)
    {
        return -EOPNOTSUPP;
    }

    if (backlog > UINT8_MAX)
    {
        backlog = UINT8_MAX;
    }

    server->u.server.backlog = backlog;
    server->u.server.pending = 0;
    server->instance_id = local_generate_instance_id();

    if (server->instance_id < 0)
    {
        return -EFAULT;
    }

    if (server->state == LOCAL_STATE_BOUND)
    {
        /* And change the server state to listing */
        server->state = LOCAL_STATE_LISTENING;
    }

    return OK;
}

/****************************************************************************
 * Name: local_connect
 *
 * Description:
 *   local_connect() connects the local socket referred to by the structure
 *   'psock' to the address specified by 'addr'. The addrlen argument
 *   specifies the size of 'addr'.  The format of the address in 'addr' is
 *   determined by the address space of the socket 'psock'.
 *
 *   If the socket 'psock' is of type SOCK_DGRAM then 'addr' is the address
 *   to which datagrams are sent by default, and the only address from which
 *   datagrams are received. If the socket is of type SOCK_STREAM or
 *   SOCK_SEQPACKET, this call attempts to make a connection to the socket
 *   that is bound to the address specified by 'addr'.
 *
 *   Generally, connection-based protocol sockets may successfully
 *   local_connect() only once; connectionless protocol sockets may use
 *   local_connect() multiple times to change their association.
 *   Connectionless sockets may dissolve the association by connecting to
 *   an address with the sa_family member of sockaddr set to AF_UNSPEC.
 *
 * Input Parameters:
 *   psock     Pointer to a socket structure initialized by psock_socket()
 *   addr      Server address (form depends on type of socket)
 *   addrlen   Length of actual 'addr'
 *
 * Returned Value:
 *   0 on success; a negated errno value on failure.  See connect() for the
 *   list of appropriate errno values to be returned.
 *
 ****************************************************************************/

static int local_connect(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    struct local_sock_s *client;
    T_UBYTE type = LOCAL_TYPE_PATHNAME;
    struct local_sock_s *server = NULL;
    struct sockaddr_un *unaddr = (struct sockaddr_un *)addr;
    const char *unpath;
    struct stat buf;
    int ret;

    if (psock == NULL || addr == NULL)
    {
        return -EINVAL;
    }

    if (strnlen(unaddr->sun_path, LOCAL_FULLPATH_LEN) == LOCAL_FULLPATH_LEN)
    {
        return -ENAMETOOLONG;
    }

    client = (struct local_sock_s *)psock->s_priv;
    if (client == NULL)
    {
        return -EINVAL;
    }

    unpath = unaddr->sun_path;

    if (addr->sa_family != AF_LOCAL || addrlen < sizeof(sa_family_t))
    {
        return -EBADF;
    }

    if (psock->s_type == SOCK_DGRAM)
    {
        ret = local_create_halfduplex(client, unaddr->sun_path);
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to create FIFO for %s: %" PRId32, client->path, ret);
            return ret;
        }
        ret = local_open_sender(client, unaddr->sun_path, true);
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to open FIFO for %s: %" PRId32, unaddr->sun_path, ret);

            local_release_halfduplex(client);
            return ret;
        }
        client->state = LOCAL_STATE_CONNECTED;
        return ret;
    }

    if (psock->s_type != SOCK_STREAM)
    {
        return -ENOSYS;
    }

    if (client->state == LOCAL_STATE_ACCEPT || client->state == LOCAL_STATE_CONNECTED)
    {
        return -EISCONN;
    }

    if (unpath[0] == '\0')
    {
        type = LOCAL_TYPE_ABSTRACT;
    }

    while ((server = local_nextconn(server)) != NULL)
    {
        /* Self found, continue */

        if (server == client)
        {
            continue;
        }

        /* Handle according to the server connection type */

        switch (server->type)
        {
        case LOCAL_TYPE_UNNAMED: /* A Unix socket that is not bound to any name */
            break;

        case LOCAL_TYPE_ABSTRACT: /* lc_path is length zero */
        case LOCAL_TYPE_PATHNAME: /* lc_path holds a null terminated string */

            /* Anything in the listener list should be a stream socket in the
             * listening state
             */

            if (server->state == LOCAL_STATE_LISTENING && server->type == type &&
                server->proto == SOCK_STREAM &&
                strncmp(server->path, unpath, UNIX_PATH_MAX - 1) == 0)
            {
                /* Bind the address and protocol */

                client->type = server->type;
                client->proto = server->proto;
                strlcpy(client->path, unpath, sizeof(client->path));
                // client->instance_id = local_generate_instance_id();

                /* The client is now bound to an address */

                client->state = LOCAL_STATE_BOUND;

                /* We have to do more for the SOCK_STREAM family */
                ret = local_stream_connect(client, server, _SS_ISNONBLOCK(client->flags));

                return ret;
            }

            break;

        default: /* Bad, memory must be corrupted */
            return -EINVAL;
        }
    }

    ret = vfs_stat(unpath, &buf, 1);
    return ret < 0 ? ret : -ECONNREFUSED;
}

/****************************************************************************
 * Name: local_poll
 *
 * Description:
 *   The standard poll() operation redirects operations on socket descriptors
 *   to local_poll which, indiectly, calls to function.
 *
 * Input Parameters:
 *   psock - An instance of the internal socket structure.
 *   fds   - The structure describing the events to be monitored, OR NULL if
 *           this is a request to stop monitoring events.
 *   setup - true: Setup up the poll; false: Teardown the poll
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/

static int local_poll(struct socket *psock, struct kpollfd *kfds, bool setup)
{
    if (setup)
    {
        /* Perform the TCP/IP poll() setup */

        return local_pollsetup(psock, kfds);
    }
    else
    {
        /* Perform the TCP/IP poll() teardown */

        return local_pollteardown(psock, kfds);
    }
}

/****************************************************************************
 * Name: local_sendmsg
 *
 * Description:
 *   Implements the sendmsg() operation for the case of the local Unix socket
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   msg      msg to send
 *   flags    Send flags
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error, a negated
 *   errno value is returned (see sendmsg() for the list of appropriate error
 *   values.
 *
 ****************************************************************************/

ssize_t local_sendmsg(struct socket *psock, struct msghdr *msg, int flags)
{
    if (msg == NULL)
    {
        return -EINVAL;
    }

    const struct sockaddr *to = msg->msg_name;
    const struct iovec *buf = msg->msg_iov;
    socklen_t tolen = msg->msg_namelen;
    size_t len = msg->msg_iovlen;

    len = to ? local_sendto(psock, buf, len, flags, to, tolen) : local_send(psock, buf, len, flags);

    return len;
}

/****************************************************************************
 * Name: local_recvmsg
 *
 * Description:
 *   recvmsg() receives messages from a local socket and may be used to
 *   receive data on a socket whether or not it is connection-oriented.
 *
 *   If from is not NULL, and the underlying protocol provides the source
 *   address, this source address is filled in. The argument fromlen
 *   initialized to the size of the buffer associated with from, and modified
 *   on return to indicate the actual size of the address stored there.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   msg      Buffer to receive the message
 *   flags    Receive flags (ignored for now)
 *
 * Returned Value:
 *   On success, returns the number of characters received. If no data is
 *   available to be received and the peer has performed an orderly shutdown,
 *   recvmsg() will return 0.  Otherwise, on errors, a negated errno value is
 *   returned (see recvmsg() for the list of appropriate error values).
 *
 ****************************************************************************/

ssize_t local_recvmsg(struct socket *psock, struct msghdr *msg, int flags)
{
    socklen_t *fromlen = &msg->msg_namelen;
    struct sockaddr *from = msg->msg_name;
    void *buf = msg->msg_iov->iov_base;
    size_t len = msg->msg_iov->iov_len;

    if (psock == NULL || msg == NULL)
    {
        return -EINVAL;
    }
    /* Check for a stream socket */

    if (psock->s_type == SOCK_STREAM)
    {
        len = psock_stream_recvfrom(psock, buf, len, flags, from, fromlen);
    }
    else if (psock->s_type == SOCK_DGRAM)
    {
        len = psock_dgram_recvfrom(psock, buf, len, flags, from, fromlen);
    }
    else
    {
        KLOG_W("ERROR: Unrecognized socket type: %d", psock->s_type);
        len = -EINVAL;
    }

    return len;
}

/****************************************************************************
 * Name: local_close
 *
 * Description:
 *   Performs the close operation on a local, Unix socket instance
 *
 * Input Parameters:
 *   psock   Socket instance
 *
 * Returned Value:
 *   0 on success; a negated errno value is returned on any failure.
 *
 * Assumptions:
 *
 ****************************************************************************/

static int local_close(struct socket *psock)
{
    if (psock == NULL)
    {
        return -EINVAL;
    }

    local_release(psock->s_priv);

    return OK;
}

/****************************************************************************
 * Name: local_ioctl
 *
 * Description:
 *   This function performs local device specific operations.
 *
 * Parameters:
 *   psock    A reference to the socket structure of the socket
 *   cmd      The ioctl command
 *   arg      The argument of the ioctl cmd
 *
 ****************************************************************************/

static int local_ioctl(struct socket *psock, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct local_sock_s *lsock;
    struct ifreq *ifr = (struct ifreq *)arg;

    if (psock == NULL)
    {
        return -EINVAL;
    }
    lsock = (struct local_sock_s *)psock->s_priv;
    if (lsock == NULL)
    {
        return -EINVAL;
    }

    switch (cmd)
    {
    case FIONBIO:
        if (lsock->infile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->infile, cmd, arg);
        }

        if (ret >= 0 && lsock->outfile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->outfile, cmd, arg);
        }
        break;
    case FIONREAD:
        if (lsock->infile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->infile, cmd, arg);
        }
        else
        {
            ret = -ENOTCONN;
        }
        break;
    case FIONWRITE:
    case FIONSPACE:
        if (lsock->outfile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->outfile, cmd, arg);
        }
        else
        {
            ret = -ENOTCONN;
        }
        break;
    case PIPEIOC_POLLINTHRD:
        if (lsock->infile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->infile, cmd, arg);
        }
        else
        {
            ret = -ENOTCONN;
        }
        break;
    case PIPEIOC_POLLOUTTHRD:
        if (lsock->outfile.f_inode != NULL)
        {
            ret = file_ioctl(&lsock->outfile, cmd, arg);
        }
        else
        {
            ret = -ENOTCONN;
        }
        break;
    case FIOC_FILEPATH:
        snprintf((char *)(uintptr_t)arg, UNIX_PATH_MAX, "local:[%s]", lsock->path);
        break;
    case BIOC_FLUSH:
        ret = -EINVAL;
        break;
    case SIOCGIFNAME:
        ret = ndev_ioctl_index_to_name(ifr);
        break;
    case SIOCGIFINDEX:
        ret = ndev_ioctl_name_to_index(ifr);
        break;
    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
}

/****************************************************************************
 * Name: local_socketpair
 *
 * Description:
 *   Create a pair of connected sockets between psocks[2]
 *
 * Parameters:
 *   psocks  A reference to the socket structure of the socket pair
 *
 ****************************************************************************/

static int local_socketpair(struct socket *psocks[2])
{
    struct local_sock_s *lsock[2];
    int ret;
    int i;
    int nonblock;

    if (psocks == NULL)
    {
        return -EINVAL;
    }

    nonblock = !!(psocks[0]->s_type & SOCK_NONBLOCK);

    for (i = 0; i < 2; i++)
    {
        lsock[i] = psocks[i]->s_priv;
        if (lsock[i] == NULL)
        {
            return -EINVAL;
        }

        snprintf(lsock[i]->path, sizeof(lsock[i]->path), "/dev/socketpair%p", psocks[0]);

        lsock[i]->proto = psocks[i]->s_type;
        lsock[i]->type = LOCAL_TYPE_PATHNAME;
        lsock[i]->state = LOCAL_STATE_BOUND;
    }

    lsock[0]->instance_id = local_generate_instance_id();
    lsock[1]->instance_id = lsock[0]->instance_id;

    /* Create the FIFOs needed for the connection */

    ret = local_create_fifos(lsock[0]);
    if (ret < 0)
    {
        goto errout;
    }

    /* Open the client-side write-only FIFO. */

    ret = local_open_client_tx(lsock[0], true);
    if (ret < 0)
    {
        goto errout;
    }

    /* Open the server-side read-only FIFO. */

    ret = local_open_server_rx(lsock[1], true);
    if (ret < 0)
    {
        goto errout;
    }

    /* Open the server-side write-only FIFO. */

    ret = local_open_server_tx(lsock[1], true);
    if (ret < 0)
    {
        goto errout;
    }

    /* Open the client-side read-only FIFO */

    ret = local_open_client_rx(lsock[0], true);
    if (ret < 0)
    {
        goto errout;
    }

    if (!nonblock)
    {
        ret = file_ioctl(&lsock[0]->infile, FIONBIO, &nonblock);
        ret = file_ioctl(&lsock[0]->outfile, FIONBIO, &nonblock);
        ret = file_ioctl(&lsock[1]->infile, FIONBIO, &nonblock);
        ret = file_ioctl(&lsock[1]->outfile, FIONBIO, &nonblock);

        if (ret < 0)
        {
            goto errout;
        }
    }

    lsock[0]->state = lsock[1]->state = LOCAL_STATE_CONNECTED;
    return OK;

errout:
    local_release_fifos(lsock[0]);
    return ret;
}

/****************************************************************************
 * Name: local_shutdown
 *
 * Description:
 *   The shutdown() function will cause all or part of a full-duplex
 *   connection on the socket associated with the file descriptor socket to
 *   be shut down.
 *
 *   The shutdown() function disables subsequent send and/or receive
 *   operations on a socket, depending on the value of the how argument.
 *
 * Input Parameters:
 *   sockfd - Specifies the file descriptor of the socket.
 *   how    - Specifies the type of shutdown. The values are as follows:
 *
 *     SHUT_RD   - Disables further receive operations.
 *     SHUT_WR   - Disables further send operations.
 *     SHUT_RDWR - Disables further send and receive operations.
 *
 ****************************************************************************/

static int local_shutdown(struct socket *psock, int how)
{
    if (psock == NULL)
    {
        return -EINVAL;
    }

    switch (psock->s_type)
    {
    case SOCK_STREAM:
    {
        struct local_sock_s *lsock = psock->s_priv;
        if (lsock == NULL)
        {
            return -EINVAL;
        }
        if (how & SHUT_RD)
        {
            if (lsock->infile.f_inode != NULL)
            {
                file_close(&lsock->infile);
                lsock->infile.f_inode = NULL;
            }
        }

        if (how & SHUT_WR)
        {
            if (lsock->outfile.f_inode != NULL)
            {
                file_close(&lsock->outfile);
                lsock->outfile.f_inode = NULL;
            }
        }
    }
        return OK;
    case SOCK_DGRAM:
        return -EOPNOTSUPP;
    default:
        return -EBADF;
    }
}

/****************************************************************************
 * Name: local_getsockopt
 *
 * Description:
 *   local_getsockopt() retrieve the value for the option specified by the
 *   'option' argument at the protocol level specified by the 'level'
 *   argument. If the size of the option value is greater than 'value_len',
 *   the value stored in the object pointed to by the 'value' argument will
 *   be silently truncated. Otherwise, the length pointed to by the
 *   'value_len' argument will be modified to indicate the actual length
 *   of the 'value'.
 *
 *   The 'level' argument specifies the protocol level of the option. To
 *   retrieve options at the socket level, specify the level argument as
 *   SOL_SOCKET.
 *
 *   See <sys/socket.h> a complete list of values for the 'option' argument.
 *
 * Input Parameters:
 *   psock     Socket structure of the socket to query
 *   level     Protocol level to set the option
 *   option    identifies the option to get
 *   value     Points to the argument value
 *   value_len The length of the argument value
 *
 ****************************************************************************/

static int local_getsockopt(struct socket *psock, int level, int option, void *value,
                            socklen_t *value_len)
{
    if (psock == NULL)
    {
        return -EINVAL;
    }

    if (level == SOL_SOCKET)
    {
        switch (option)
        {
        case SO_SNDBUF:
        {
            if (*value_len != sizeof(int))
            {
                return -EINVAL;
            }

            *(int *)value = LOCAL_SEND_LIMIT;
            return OK;
        }
        }
    }

    return -ENOPROTOOPT;
}

/****************************************************************************
 * Name: local_setsockopt
 *
 * Description:
 *   local_setsockopt() sets the option specified by the 'option' argument,
 *   at the protocol level specified by the 'level' argument, to the value
 *   pointed to by the 'value' argument for the usrsock connection.
 *
 *   The 'level' argument specifies the protocol level of the option. To set
 *   options at the socket level, specify the level argument as SOL_SOCKET.
 *
 *   See <sys/socket.h> a complete list of values for the 'option' argument.
 *
 * Input Parameters:
 *   psock     Socket structure of the socket to query
 *   level     Protocol level to set the option
 *   option    identifies the option to set
 *   value     Points to the argument value
 *   value_len The length of the argument value
 *
 ****************************************************************************/

static int local_setsockopt(struct socket *psock, int level, int option, const void *value,
                            socklen_t value_len)
{
    return -ENOPROTOOPT;
}

static const struct sa_type local_supp_sa_types[] = {
    {.family = AF_LOCAL, .type = SOCK_STREAM, .protocol = 0},
    {.family = AF_LOCAL, .type = SOCK_DGRAM, .protocol = 0},

};

static struct sock_intf_s local_sock_if = {
    .type = local_supp_sa_types,
    .type_count = array_size(local_supp_sa_types),
    .si_setup = local_setup,
    .si_sockcaps = local_sockcaps,
    .si_addref = local_addref,
    .si_bind = local_bind,
    .si_getsockname = local_getsockname,
    .si_getpeername = local_getpeername,
    .si_listen = local_listen,
    .si_connect = local_connect,
    .si_accept = local_accept,
    .si_poll = local_poll,
    .si_sendmsg = local_sendmsg,
    .si_recvmsg = local_recvmsg,
    .si_close = local_close,
    .si_ioctl = local_ioctl,
    .si_socketpair = local_socketpair,
    .si_shutdown = local_shutdown,
    .si_getsockopt = local_getsockopt,
    .si_setsockopt = local_setsockopt,
};

int register_local_sockif()
{
    KLOG_I("local socks");
    INIT_LIST_HEAD(&g_local_connections);
    return register_sockif(&local_sock_if);
}
INIT_EXPORT_SUBSYS(register_local_sockif, "init local socket interface");
