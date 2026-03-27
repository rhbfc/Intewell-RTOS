/****************************************************************************
 * net/local/local_fifo.c
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
 * Name: local_format_name
 *
 * Description:
 *   Format the name of the half duplex FIFO.
 *
 ****************************************************************************/

static void local_format_name(const char *inpath, char *outpath, const char *suffix, uint32_t id)
{
    snprintf(outpath, UNIX_PATH_MAX, "%s%s%s%u", CONFIG_NET_LOCAL_VFS_PATH, inpath, suffix, id);
}

/****************************************************************************
 * Name: local_cs_name
 *
 * Description:
 *   Create the name of the client-to-server FIFO.
 *
 ****************************************************************************/

static void local_cs_name(struct local_sock_s *lsock, char *path)
{
    if (lsock == NULL)
    {
        return;
    }
    local_format_name(lsock->path, path, LOCAL_CS_SUFFIX, lsock->instance_id);
}

/****************************************************************************
 * Name: local_sc_name
 *
 * Description:
 *   Create the name of the server-to-client FIFO.
 *
 ****************************************************************************/

static void local_sc_name(struct local_sock_s *lsock, char *path)
{
    if (lsock == NULL)
    {
        return;
    }
    local_format_name(lsock->path, path, LOCAL_SC_SUFFIX, lsock->instance_id);
}

/****************************************************************************
 * Name: local_hd_name
 *
 * Description:
 *   Create the name of the half duplex, datagram FIFO.
 *
 ****************************************************************************/

static void local_hd_name(const char *inpath, char *outpath)
{
    local_format_name(inpath, outpath, LOCAL_HD_SUFFIX, 0);
}

/****************************************************************************
 * Name: local_fifo_exists
 *
 * Description:
 *   Check if a FIFO exists.
 *
 ****************************************************************************/

static bool local_fifo_exists(const char *path)
{
    struct stat buf;
    int ret;

    /* Create the client-to-server FIFO */

    ret = vfs_stat(path, &buf, 1);
    if (ret < 0)
    {
        return false;
    }

    /* Return true if what we found is a FIFO. What if it is something else?
     * In that case, we will return false and mkfifo() will fail.
     */

    return S_ISFIFO(buf.st_mode);
}

/****************************************************************************
 * Name: local_create_fifo
 *
 * Description:
 *   Create the one FIFO.
 *
 ****************************************************************************/
extern int ttos_mkfifo(const char *pathname, mode_t mode, size_t bufsize);
static int local_create_fifo(const char *path)
{
    int ret;

    /* Create the client-to-server FIFO if it does not already exist. */

    if (!local_fifo_exists(path))
    {
        ret = ttos_mkfifo(path, 0644, 32 * 1024);
        if (ret < 0)
        {
            if (ret == -EEXIST)
            {
                return OK;
            }
            KLOG_W("ERROR: Failed to create FIFO %s: %d", path, ret);
            return ret;
        }
    }

    /* The FIFO (or some character driver) exists at PATH or we successfully
     * created the FIFO at that location.
     */

    return OK;
}

/****************************************************************************
 * Name: local_release_fifo
 *
 * Description:
 *   Release a reference from one of the FIFOs used in a connection.
 *
 ****************************************************************************/

static int local_release_fifo(const char *path)
{
    int ret;

    /* Unlink the client-to-server FIFO if it exists. */

    if (local_fifo_exists(path))
    {
        /* Un-linking the FIFO removes the FIFO from the namespace.  It will
         * also mark the FIFO device "unlinked".  When all of the open
         * references to the FIFO device are closed, the resources consumed
         * by the device instance will also be freed.
         */

        ret = vfs_unlink(path);
        if (ret < 0)
        {
            KLOG_W("ERROR: Failed to unlink FIFO %s: %d", path, ret);
            return ret;
        }
    }

    /* The FIFO does not exist or we successfully unlinked it. */

    return OK;
}

/****************************************************************************
 * Name: local_rx_open
 *
 * Description:
 *   Open a FIFO for read-only access.
 *
 ****************************************************************************/

static int local_rx_open(struct local_sock_s *lsock, const char *path, bool nonblock)
{
    int oflags = nonblock ? O_RDONLY | O_NONBLOCK : O_RDONLY;
    int ret;

    if (lsock == NULL)
    {
        return -EINVAL;
    }
    ret = file_open(&lsock->infile, path, oflags | O_CLOEXEC);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed on file_open %s for reading: %d", path, ret);
        /* Map the error code to something consistent with the return
         * error codes from connect():
         *
         * If error is ENOENT, meaning that the FIFO does exist,
         * return EFAULT meaning that the socket structure address is
         * outside the user's address space.
         */

        return ret == -ENOENT ? -EFAULT : ret;
    }

    return OK;
}

/****************************************************************************
 * Name: local_tx_open
 *
 * Description:
 *   Open a FIFO for write-only access.
 *
 ****************************************************************************/

static int local_tx_open(struct local_sock_s *lsock, const char *path, bool nonblock)
{
    int oflags = nonblock ? O_WRONLY | O_NONBLOCK : O_WRONLY;
    int ret;

    if (lsock == NULL)
    {
        return -EINVAL;
    }
    ret = file_open(&lsock->outfile, path, oflags | O_CLOEXEC);
    if (ret < 0)
    {
        KLOG_W("ERROR: Failed on file_open %s for writing: %d", path, ret);

        /* Map the error code to something consistent with the return
         * error codes from connect():
         *
         * If error is ENOENT, meaning that the FIFO does exist,
         * return EFAULT meaning that the socket structure address is
         * outside the user's address space.
         */

        return ret == -ENOENT ? -EFAULT : ret;
    }

    /* Clear O_NONBLOCK if it's meant to be blocking */

    if (nonblock == false)
    {
        ret = nonblock;
        ret = file_ioctl(&lsock->outfile, FIONBIO, &ret);
        if (ret < 0)
        {
            return ret;
        }
    }

    return OK;
}
/****************************************************************************
 * Name: local_create_fifos
 *
 * Description:
 *   Create the FIFO pair needed for a SOCK_STREAM connection.
 *
 ****************************************************************************/

int local_create_fifos(struct local_sock_s *lsock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Create the client-to-server FIFO if it does not already exist. */

    local_cs_name(lsock, path);
    ret = local_create_fifo(path);
    if (ret >= 0)
    {
        /* Create the server-to-client FIFO if it does not already exist. */
        local_sc_name(lsock, path);
        ret = local_create_fifo(path);
    }

    return ret;
}

/****************************************************************************
 * Name: local_create_halfduplex
 *
 * Description:
 *   Create the half-duplex FIFO needed for SOCK_DGRAM communication.
 *
 ****************************************************************************/

int local_create_halfduplex(struct local_sock_s *lsock, const char *path)
{
    char fullpath[UNIX_PATH_MAX];

    /* Create the half duplex FIFO if it does not already exist. */

    local_hd_name(path, fullpath);
    return local_create_fifo(fullpath);
}

/****************************************************************************
 * Name: local_release_fifos
 *
 * Description:
 *   Release references to the FIFO pair used for a SOCK_STREAM connection.
 *
 ****************************************************************************/

int local_release_fifos(struct local_sock_s *lsock)
{
    char path[UNIX_PATH_MAX];
    int ret1;
    int ret2;

    /* Destroy the server-to-client FIFO if it exists. */

    local_sc_name(lsock, path);
    ret1 = local_release_fifo(path);

    /* Destroy the client-to-server FIFO if it exists. */

    local_cs_name(lsock, path);
    ret2 = local_release_fifo(path);

    /* Return a failure if one occurred. */

    return ret1 < 0 ? ret1 : ret2;
}

/****************************************************************************
 * Name: local_release_halfduplex
 *
 * Description:
 *   Release a reference to the FIFO used for SOCK_DGRAM communication
 *
 ****************************************************************************/

int local_release_halfduplex(struct local_sock_s *lsock)
{
#if 1
    /* REVISIT: We need to think about this carefully.  Unlike the connection-
     * oriented Unix domain socket, we don't really know the best time to
     * release the FIFO resource.  It would be extremely inefficient to create
     * and destroy the FIFO on each packet. But, on the other hand, failing
     * to destroy the FIFO will leave the FIFO resources in place after the
     * communications have completed.
     *
     * I am thinking that there should be something like a timer.  The timer
     * would be started at the completion of each transfer and cancelled at
     * the beginning of each transfer.  If the timer expires, then the FIFO
     * would be destroyed.
     */

    /* #warning Missing logic */

    return OK;

#else
    char path[UNIX_PATH_MAX];

    /* Destroy the half duplex FIFO if it exists. */

    local_hd_name(lsock->lc_path, path);
    return local_release_fifo(path);
#endif
}

/****************************************************************************
 * Name: local_open_client_rx
 *
 * Description:
 *   Open the client-side of the server-to-client FIFO.
 *
 ****************************************************************************/

int local_open_client_rx(struct local_sock_s *client, bool nonblock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Get the server-to-client path name */

    local_sc_name(client, path);

    /* Then open the file for read-only access */

    ret = local_rx_open(client, path, nonblock);
    if (ret == OK)
    {
        /* Policy: Free FIFO resources when the last reference is closed */

        // ret = local_set_policy(&client->infile, 0);
    }

    return ret;
}

/****************************************************************************
 * Name: local_open_client_tx
 *
 * Description:
 *   Open the client-side of the client-to-server FIFO.
 *
 ****************************************************************************/

int local_open_client_tx(struct local_sock_s *client, bool nonblock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Get the client-to-server path name */

    local_cs_name(client, path);

    /* Then open the file for write-only access */

    ret = local_tx_open(client, path, nonblock);

    return ret;
}

/****************************************************************************
 * Name: local_open_server_rx
 *
 * Description:
 *   Open the server-side of the client-to-server FIFO.
 *
 ****************************************************************************/

int local_open_server_rx(struct local_sock_s *server, bool nonblock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Get the client-to-server path name */
    local_cs_name(server, path);

    /* Then open the file for read-only access */

    ret = local_rx_open(server, path, nonblock);
    if (ret == OK)
    {
        /* Policy: Free FIFO resources when the last reference is closed */

        // ret = local_set_policy(&server->infile, 0);
    }

    return ret;
}

/****************************************************************************
 * Name: local_open_server_tx
 *
 * Description:
 *   Open the server-side of the server-to-client FIFO.
 *
 ****************************************************************************/

int local_open_server_tx(struct local_sock_s *server, bool nonblock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Get the server-to-client path name */

    local_sc_name(server, path);

    /* Then open the file for write-only access */

    ret = local_tx_open(server, path, nonblock);
    if (ret == OK)
    {
        /* Policy: Free FIFO resources when the last reference is closed */

        // ret = local_set_policy(&server->outfile, 0);
    }

    return ret;
}

/****************************************************************************
 * Name: local_open_receiver
 *
 * Description:
 *   Open the receiving side of the half duplex FIFO.
 *
 ****************************************************************************/

int local_open_receiver(struct local_sock_s *lsock, bool nonblock)
{
    char path[UNIX_PATH_MAX];
    int ret;

    /* Get the server-to-client path name */

    local_hd_name(lsock->path, path);

    /* Then open the file for read-only access */

    ret = local_rx_open(lsock, path, nonblock);
    return ret;
}

/****************************************************************************
 * Name: local_open_sender
 *
 * Description:
 *   Open the sending side of the half duplex FIFO.
 *
 ****************************************************************************/

int local_open_sender(struct local_sock_s *lsock, const char *path, bool nonblock)
{
    char fullpath[UNIX_PATH_MAX];
    int ret;

    /* Get the server-to-client path name */

    local_hd_name(path, fullpath);

    /* Then open the file for write-only access */

    ret = local_tx_open(lsock, fullpath, nonblock);
    return ret;
}

/****************************************************************************
 * Name: local_set_nonblocking
 *
 * Description:
 *   Set the local conntion to nonblocking mode
 *
 ****************************************************************************/

int local_set_nonblocking(struct local_sock_s *lsock)
{
    int nonblock = 1;
    int ret;

    if (lsock == NULL)
    {
        return -EINVAL;
    }
    /* Set the conn to nonblocking mode */

    ret = file_ioctl(&lsock->infile, FIONBIO, &nonblock);
    ret |= file_ioctl(&lsock->outfile, FIONBIO, &nonblock);

    if (ret < 0)
    {
        KLOG_W("ERROR: Failed to set the conn to nonblocking mode: %d", ret);
    }

    return ret;
}
