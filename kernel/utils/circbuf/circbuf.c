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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <assert.h>
#include <circbuf.h>
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: circbuf_init
 *
 * Description:
 *   Initialize a circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   base  - A pointer to circular buffer's internal buffer. It can be
 *           provided by caller because sometimes the creation of buffer
 *           is special or needs to preallocated, eg: DMA buffer.
 *           If NULL, a buffer of the given size will be allocated.
 *   bytes - The size of the internal buffer.
 *
 * Returned Value:
 *   Zero on success; A negated errno value is returned on any failure.
 *
 ****************************************************************************/

int circbuf_init (circbuf_t *circ, void *base, size_t bytes)
{
    DEBUGASSERT (circ);
    DEBUGASSERT (!base || bytes);

    circ->external = !!base;

    if (!base && bytes)
    {
        base = malloc (bytes);
        if (!base)
        {
            return -ENOMEM;
        }
    }

    circ->base = base;
    circ->size = bytes;
    circ->head = 0;
    circ->tail = 0;

    return 0;
}

/****************************************************************************
 * Name: circbuf_resize
 *
 * Description:
 *   Resize a circular buffer (change buffer size).
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   bytes - The size of the internal buffer.
 *
 * Returned Value:
 *   Zero on success; A negated errno value is returned on any failure.
 *
 ****************************************************************************/

int circbuf_resize (circbuf_t *circ, size_t bytes)
{
    void  *tmp = NULL;
    size_t len = 0;

    DEBUGASSERT (circ);
    DEBUGASSERT (!circ->external);
    if (bytes == circ->size)
    {
        return 0;
    }

    if (bytes)
    {
        tmp = malloc (bytes);
        if (!tmp)
        {
            return -ENOMEM;
        }

        len = circbuf_used (circ);
        if (bytes < len)
        {
            circbuf_skip (circ, len - bytes);
            len = bytes;
        }

        circbuf_read (circ, tmp, len);
    }

    free (circ->base);

    circ->base = tmp;
    circ->size = bytes;
    circ->head = len;
    circ->tail = 0;

    return 0;
}

/****************************************************************************
 * Name: circbuf_reset
 *
 * Description:
 *   Remove the entire circular buffer content.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

void circbuf_reset (circbuf_t *circ)
{
    DEBUGASSERT (circ);
    circ->head = circ->tail = 0;
}

/****************************************************************************
 * Name: circbuf_uninit
 *
 * Description:
 *   Free the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

void circbuf_uninit (circbuf_t *circ)
{
    DEBUGASSERT (circ);

    if (!circ->external)
    {
        free (circ->base);
    }

    memset (circ, 0, sizeof (*circ));
}

/****************************************************************************
 * Name: circbuf_size
 *
 * Description:
 *   Return size of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_size (circbuf_t *circ)
{
    DEBUGASSERT (circ);
    return circ->size;
}

/****************************************************************************
 * Name: circbuf_used
 *
 * Description:
 *   Return the used bytes of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_used (circbuf_t *circ)
{
    DEBUGASSERT (circ);
    return circ->head - circ->tail;
}

/****************************************************************************
 * Name: circbuf_space
 *
 * Description:
 *   Return the remaining space of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_space (circbuf_t *circ)
{
    return circbuf_size (circ) - circbuf_used (circ);
}

/****************************************************************************
 * Name: circbuf_is_init
 *
 * Description:
 *   Return true if the circular buffer had been initialized.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_init (circbuf_t *circ)
{
    return !!circ->base;
}

/****************************************************************************
 * Name: circbuf_is_empty
 *
 * Description:
 *   Return true if the circular buffer is empty.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_empty (circbuf_t *circ)
{
    return !circbuf_used (circ);
}

/****************************************************************************
 * Name: circbuf_is_full
 *
 * Description:
 *   Return true if the circular buffer is full.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_full (circbuf_t *circ)
{
    return !circbuf_space (circ);
}

/****************************************************************************
 * Name: circbuf_peekat
 *
 * Description:
 *   Get data specified position from the circular buffer without removing
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   pos   - Position to read.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the peek data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_peekat (circbuf_t *circ, size_t pos, void *dst, size_t bytes)
{
    size_t len;
    size_t off;

    DEBUGASSERT (circ);

    if (!circ->size)
    {
        return 0;
    }

    if (circ->head - pos > circ->head - circ->tail)
    {
        pos = circ->tail;
    }

    len = circ->head - pos;
    off = pos % circ->size;

    if (bytes > len)
    {
        bytes = len;
    }

    len = circ->size - off;
    if (bytes < len)
    {
        len = bytes;
    }

    memcpy (dst, (char *)circ->base + off, len);
    memcpy ((char *)dst + len, circ->base, bytes - len);

    return bytes;
}

/****************************************************************************
 * Name: circbuf_peek
 *
 * Description:
 *   Get data from the circular buffer without removing
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the peek data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_peek (circbuf_t *circ, void *dst, size_t bytes)
{
    return circbuf_peekat (circ, circ->tail, dst, bytes);
}

/****************************************************************************
 * Name: circbuf_read
 *
 * Description:
 *   Get data from the circular buffer.
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the read data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_read (circbuf_t *circ, void *dst, size_t bytes)
{
    DEBUGASSERT (circ);
    DEBUGASSERT (dst || !bytes);

    bytes = circbuf_peek (circ, dst, bytes);
    circ->tail += bytes;

    return bytes;
}

/****************************************************************************
 * Name: circbuf_skip
 *
 * Description:
 *   Skip data from the circular buffer.
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   bytes - Number of bytes to skip.
 *
 * Returned Value:
 *   The bytes of get data is returned if the skip data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_skip (circbuf_t *circ, size_t bytes)
{
    size_t len;

    DEBUGASSERT (circ);

    len = circbuf_used (circ);

    if (bytes > len)
    {
        bytes = len;
    }

    circ->tail += bytes;

    return bytes;
}

/****************************************************************************
 * Name: circbuf_write
 *
 * Description:
 *   Write data to the circular buffer.
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   src   - The data to be added.
 *   bytes - Number of bytes to be added.
 *
 * Returned Value:
 *   The bytes of get data is returned if the write data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_write (circbuf_t *circ, const void *src, size_t bytes)
{
    size_t space;
    size_t off;

    DEBUGASSERT (circ);
    DEBUGASSERT (src || !bytes);

    if (!circ->size)
    {
        return 0;
    }

    space = circbuf_space (circ);
    off   = circ->head % circ->size;
    if (bytes > space)
    {
        bytes = space;
    }

    space = circ->size - off;
    if (bytes < space)
    {
        space = bytes;
    }

    memcpy ((char *)circ->base + off, src, space);
    memcpy (circ->base, (char *)src + space, bytes - space);
    circ->head += bytes;

    return bytes;
}

/****************************************************************************
 * Name: circbuf_overwrite
 *
 * Description:
 *   Write data to the circular buffer. It can overwrite old data when
 *   circular buffer don't have enough space to store data.
 *
 * Note:
 *   Usage circbuf_overwrite () is dangerous. It should be only called
 *   when the buffer is exclusived locked or when it is secured that no
 *   other thread is accessing the buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   src   - The data to be added.
 *   bytes - Number of bytes to be added.
 *
 * Returned Value:
 *   The bytes length of overwrite is returned if it's successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_overwrite (circbuf_t *circ, const void *src, size_t bytes)
{
    size_t overwrite = 0;
    size_t skip      = 0;
    size_t space;
    size_t off;

    DEBUGASSERT (circ);
    DEBUGASSERT (src || !bytes);

    if (!circ->size)
    {
        return 0;
    }

    if (bytes > circ->size)
    {
        skip  = bytes - circ->size;
        src   = (const void *)((char *)src + skip);
        bytes = circ->size;
    }

    space = circbuf_space (circ);
    if (bytes > space)
    {
        overwrite = bytes - space + skip;
    }

    circ->head += skip;
    off   = circ->head % circ->size;
    space = circ->size - off;
    if (bytes < space)
    {
        space = bytes;
    }

    memcpy ((char *)circ->base + off, src, space);
    memcpy (circ->base, (char *)src + space, bytes - space);
    circ->head += bytes;
    circ->tail += overwrite;

    return overwrite;
}

/****************************************************************************
 * Name: circbuf_get_writeptr
 *
 * Description:
 *   Get the write pointer of the circbuf.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   size  - Returns the maximum size that can be written consecutively
 *
 * Returned Value:
 *   The write pointer of the circbuf.
 *
 ****************************************************************************/

void *circbuf_get_writeptr (circbuf_t *circ, size_t *size)
{
    size_t off;
    size_t pos;

    DEBUGASSERT (circ);

    off = circ->head % circ->size;
    pos = circ->tail % circ->size;
    if (off >= pos)
    {
        *size = circ->size - off;
    }
    else
    {
        *size = pos - off;
    }

    return (char *)circ->base + off;
}

/****************************************************************************
 * Name: circbuf_get_readptr
 *
 * Description:
 *   Get the read pointer of the circbuf.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   size  - Returns the maximum size that can be read consecutively.
 *
 * Returned Value:
 *   The read pointer of the circbuf.
 *
 ****************************************************************************/

void *circbuf_get_readptr (circbuf_t *circ, size_t *size)
{
    size_t off;
    size_t pos;

    DEBUGASSERT (circ);

    off = circ->head % circ->size;
    pos = circ->tail % circ->size;
    if (pos > off)
    {
        *size = circ->size - pos;
    }
    else
    {
        *size = off - pos;
    }

    return (char *)circ->base + pos;
}

/****************************************************************************
 * Name: circbuf_writecommit
 *
 * Description:
 *   After writing data using the buf returned by circbuf_writebuf,
 *   you need to use this function to update the internal structure
 *   of cricbuf.
 *
 * Input Parameters:
 *   circ        - Address of the circular buffer to be used.
 *   writtensize - The data that has been written to the buffer.
 *
 ****************************************************************************/

void circbuf_writecommit (circbuf_t *circ, size_t writtensize)
{
    DEBUGASSERT (circ);
    circ->head += writtensize;
}

/****************************************************************************
 * Name: circbuf_readcommit
 *
 * Description:
 *   After reading data using the buf returned by circbuf_readbuf,
 *   you need to use this function to update the internal structure
 *   of cricbuf.
 *
 * Input Parameters:
 *   circ     - Address of the circular buffer to be used.
 *   readsize - The data that has been read to the buffer.
 *
 ****************************************************************************/

void circbuf_readcommit (circbuf_t *circ, size_t readsize)
{
    DEBUGASSERT (circ);
    circ->tail += readsize;
}
