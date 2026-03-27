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

#ifndef __NET_LOCAL_LOCAL_H
#define __NET_LOCAL_LOCAL_H

#include <assert.h>
#include <commonUtils.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/kpoll.h>
#include <limits.h>
#include <list.h>
#include <net/net.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ttos.h>

#define OK 0
#define UNIX_PATH_MAX 108
#define LOCAL_NPOLLWAITERS 2
#define LOCAL_NCONTROLFDS 4

#ifndef CONFIG_DEV_FIFO_SIZE
#define CONFIG_DEV_FIFO_SIZE 1024
#endif
#define LOCAL_SEND_LIMIT (CONFIG_DEV_FIFO_SIZE - sizeof(uint16_t))

#define LOCAL_CS_SUFFIX "CS" /* Name of the client-to-server FIFO */
#define LOCAL_SC_SUFFIX "SC" /* Name of the server-to-client FIFO */
#define LOCAL_HD_SUFFIX "HD" /* Name of the half duplex datagram FIFO */
#define LOCAL_SUFFIX_LEN 2
#define LOCAL_FULLPATH_LEN                                                                         \
    (UNIX_PATH_MAX - (LOCAL_SUFFIX_LEN + 10)) /* 10: U32MAX (0xFFFFFFFF) digits */

enum local_path_type
{
    LOCAL_TYPE_UNNAMED = 0, /* A Unix socket that is not bound to any name */
    LOCAL_TYPE_PATHNAME,    /* path holds a null terminated string */
    LOCAL_TYPE_ABSTRACT     /* path is length zero */
};

enum local_state
{
    /* Common states */

    LOCAL_STATE_UNBOUND = 0, /* Created by socket, but not bound */
    LOCAL_STATE_BOUND,       /* Bound to an path */

    /* SOCK_STREAM server only */

    LOCAL_STATE_LISTENING, /* Server listening for connections */

    /* SOCK_STREAM peers only */

    LOCAL_STATE_ACCEPT,      /* Client waiting for a connection */
    LOCAL_STATE_CONNECTED,   /* Peer connected */
    LOCAL_STATE_DISCONNECTED /* Peer disconnected */
};

typedef struct local_sock_s
{
    struct list_node node;

    uint16_t type;
    uint16_t flags;
    uint16_t proto; /* SOCK_STREAM or SOCK_DGRAM */

    enum local_state state;
    uint32_t instance_id;
    uint32_t pktlen;
    struct file infile;  /* File for read-only FIFO (peers) */
    struct file outfile; /* File descriptor of write-only FIFO (peers) */

    MUTEX_ID sendlock; /* Make sending multi-thread safe */
    MUTEX_ID polllock; /* Lock for net poll */
    SEMA_ID waitsem;   /* Use to wait for a connection to be accepted */

    char path[UNIX_PATH_MAX]; /* Path assigned by bind() */
    /* The following is a list if poll structures of threads waiting for
     * socket events.
     */

    struct kpollfd *event_fds[2];
    struct kpollfd inout_fds[4];

    struct local_sock_s *peer; /* Peer connection instance */

    union {
        /* Fields unique to the SOCK_STREAM server side */

        struct
        {
            struct list_node waiters; /* List of connections waiting to be accepted */
            uint8_t pending;          /* Number of pending connections */
            uint8_t backlog;          /* Maximum number of pending connections */
        } server;

        /* Fields unique to the connecting accept side */

        struct
        {
            struct list_node waiter; /* Linked to the lc_waiters lists */
            uint8_t conn_state;
        } accept;
    } u;

    ttos_spinlock_t server_waiters_lock;
} local_sock_t;

/****************************************************************************
 * Public Type Definitions
 ****************************************************************************/

/* A UNIX domain socket address is represented in the following structure.
 * This structure must be cast compatible with struct sockaddr.
 */

struct sockaddr_un
{
    sa_family_t sun_family;       /* AF_UNIX */
    char sun_path[UNIX_PATH_MAX]; /* pathname */
};

extern struct list_node g_local_connections;
extern ttos_spinlock_t g_local_conn_lock;

struct local_sock_s *local_alloc(void);
int local_alloc_accept(struct local_sock_s *server, struct local_sock_s *client,
                       struct local_sock_s **accept);
void local_free(struct local_sock_s *lsock);
int local_sync(struct file *filep);
void local_addref(struct socket *psock);
struct local_sock_s *local_nextconn(struct local_sock_s *lsock);
struct local_sock_s *local_peerconn(struct local_sock_s *lsock);
int psock_local_bind(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen);
int psock_local_connect(struct socket *psock, const struct sockaddr *addr);
ssize_t psock_stream_recvfrom(struct socket *psock, void *buf, size_t len, int flags,
                              struct sockaddr *from, socklen_t *fromlen);
ssize_t psock_dgram_recvfrom(struct socket *psock, void *buf, size_t len, int flags,
                             struct sockaddr *from, socklen_t *fromlen);
int local_stream_connect(struct local_sock_s *client, struct local_sock_s *server, bool nonblock);
uint32_t local_generate_instance_id(void);
int local_release(struct local_sock_s *lsock);
int local_listen(struct socket *psock, int backlog);
ssize_t local_sendto(struct socket *psock, const struct iovec *buf, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen);
ssize_t local_send(struct socket *psock, const struct iovec *buf, size_t len, int flags);
int local_send_packet(struct file *filep, const struct iovec *buf, size_t len, bool preamble);
ssize_t local_sendmsg(struct socket *psock, struct msghdr *msg, int flags);
ssize_t local_recvmsg(struct socket *psock, struct msghdr *msg, int flags);
int local_fifo_read(struct file *filep, uint8_t *buf, size_t *len, bool once);
int local_accept(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen,
                 struct socket *newsock, int flags);
int local_getaddr(struct local_sock_s *lsock, struct sockaddr *addr, socklen_t *addrlen);
int local_create_fifos(struct local_sock_s *lsock);
int local_create_halfduplex(struct local_sock_s *lsock, const char *path);
int local_release_fifos(struct local_sock_s *lsock);
int local_release_halfduplex(struct local_sock_s *lsock);
int local_open_client_rx(struct local_sock_s *client, bool nonblock);
int local_open_client_tx(struct local_sock_s *client, bool nonblock);
int local_open_server_rx(struct local_sock_s *server, bool nonblock);
int local_open_server_tx(struct local_sock_s *server, bool nonblock);
int local_open_receiver(struct local_sock_s *lsock, bool nonblock);
int local_open_sender(struct local_sock_s *lsock, const char *path, bool nonblock);
void local_event_pollnotify(struct local_sock_s *lsock, uint32_t eventset);
int local_pollsetup(struct socket *psock, struct kpollfd *fds);
int local_pollteardown(struct socket *psock, struct kpollfd *fds);
int local_set_pollthreshold(struct local_sock_s *lsock, unsigned long threshold);
int local_set_nonblocking(struct local_sock_s *lsock);
#endif
