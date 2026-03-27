/****************************************************************************
 * net/local/local_sendpacket.c
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
 * Name: local_fifo_write
 *
 * Description:
 *   Write a data on the write-only FIFO.
 *
 * Input Parameters:
 *   filep    File structure of write-only FIFO.
 *   buf      Data to send
 *   len      Length of data to send
 *
 * Returned Value:
 *   On success, the number of bytes written are returned (zero indicates
 *   nothing was written).  On any failure, a negated errno value is returned
 *
 ****************************************************************************/

static int local_fifo_write(struct file *filep, const uint8_t *buf,
                            size_t len)
{
  ssize_t nwritten = 0;
  ssize_t ret = 0;

  while (len != nwritten)
  {
    ret = file_write(filep, buf + nwritten, len - nwritten);
    if (ret < 0)
    {
      if (ret == -EINTR)
      {
        continue;
      }
      else if (ret == -EAGAIN)
      {
        break;
      }
      else
      {
        KLOG_W("ERROR: file_write failed: %" PRIdPTR, ret);
        break;
      }
    }

    nwritten += ret;
  }

  return nwritten > 0 ? nwritten : ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: local_send_packet
 *
 * Description:
 *   Send a packet on the write-only FIFO.
 *
 * Input Parameters:
 *   filep    File structure of write-only FIFO.
 *   buf      Data to send
 *   len      Length of data to send
 *   preamble Flag to indicate the preamble sync header assembly
 *
 * Returned Value:
 *   Packet length is returned on success; a negated errno value is returned
 *   on any failure.
 *
 ****************************************************************************/

int local_send_packet(struct file *filep, const struct iovec *buf,
                      size_t len, bool preamble)
{
  const struct iovec *end;
  const struct iovec *iov;
  int ret = -EINVAL;
  uint16_t len16;

  if (buf == NULL)
  {
    return -EINVAL;
  }

  end = buf + len;
  if (preamble)
  {
    /* Send the packet length */

    for (len16 = 0, iov = buf; iov != end; iov++)
    {
      len16 += iov->iov_len;
    }

    if (len16 > LOCAL_SEND_LIMIT)
    {
      KLOG_W("ERROR: Packet is too big: %d", len16);
      return -EMSGSIZE;
    }

    ret = local_fifo_write(filep, (const uint8_t *)&len16,
                            sizeof(uint16_t));
    if (ret != sizeof(uint16_t))
    {
      return ret;
    }
  }

  for (len16 = 0, iov = buf; iov != end; iov++)
  {
    ret = local_fifo_write(filep, iov->iov_base, iov->iov_len);
    if (ret < 0)
    {
      break;
    }

    if (ret > 0)
    {
      len16 += ret;
      if (ret != iov->iov_len)
      {
        break;
      }
    }
  }

  return len16 > 0 ? len16 : ret;
}
