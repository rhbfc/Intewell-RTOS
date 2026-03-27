/****************************************************************************
 * include/fs/kpoll.h
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

#ifndef __KPOLL_H
#define __KPOLL_H

#define _GNU_SOURCE 1
#include <atomic.h>
#include <poll.h>
#include <stdbool.h>
#include <ttosUtils.h>

typedef T_TTOS_Task_Queue_Control wait_queue_head_t;

#define POLLFILE 0x800

struct kpollfd;
typedef void (*pollcb_t)(struct kpollfd *fds);

struct kpollfd
{
    /* Standard fields */
    struct pollfd pollfd;

    void *arg;
    pollcb_t cb;
    void *priv;

    /* Lightweight, local refcount for IRQ-safe kpoll lifecycle */
    atomic_t refcnt;
    wait_queue_head_t wq;
};

struct aio_info
{
    bool aio_enable;
    int pid;
};

int kpoll(struct kpollfd *fds, nfds_t nfds, int timeout);
void kpoll_notify(struct kpollfd **afds, int nfds, short eventset, struct aio_info *aio);

#define POLLMASK                                                                                   \
    (POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP | POLLNVAL | POLLRDNORM | POLLRDBAND |         \
     POLLWRNORM | POLLWRBAND | POLLMSG | POLLRDHUP)

#define POLLALWAYS (0x10000) /* For not conflict with Linux */

#endif /* __KPOLL_H */
