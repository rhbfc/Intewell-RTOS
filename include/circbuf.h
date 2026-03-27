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

/*
 * @file： circbuf.h
 * @brief：
 *	    <li>环缓冲相关函数声明类型定义及宏定义。</li>
 */
#ifndef _CIRCBUF_H
#define _CIRCBUF_H

/************************头文件********************************/
#include <bits/alltypes.h>
#include <stdbool.h>
#include <system/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#define DEBUGASSERT(x) assert(x)

/************************类型定义******************************/
typedef struct circbuf {
    void  *base;     /* The pointer to buffer space */
    size_t size;     /* The size of buffer space */
    size_t head;     /* The head of buffer space */
    size_t tail;     /* The tail of buffer space */
    bool   external; /* The flag for external buffer */
} circbuf_t;

/************************接口声明******************************/
int     circbuf_init (circbuf_t *circ, void *base, size_t bytes);
int     circbuf_resize (circbuf_t *circ, size_t bytes);
void    circbuf_uninit (circbuf_t *circ);
void    circbuf_reset (circbuf_t *circ);
bool    circbuf_is_init (circbuf_t *circ);
bool    circbuf_is_full (circbuf_t *circ);
bool    circbuf_is_empty (circbuf_t *circ);
size_t  circbuf_size (circbuf_t *circ);
size_t  circbuf_used (circbuf_t *circ);
size_t  circbuf_space (circbuf_t *circ);
ssize_t circbuf_peekat (circbuf_t *circ, size_t pos, void *dst, size_t bytes);
ssize_t circbuf_peek (circbuf_t *circ, void *dst, size_t bytes);
ssize_t circbuf_read (circbuf_t *circ, void *dst, size_t bytes);
ssize_t circbuf_skip (circbuf_t *circ, size_t bytes);
ssize_t circbuf_write (circbuf_t *circ, const void *src, size_t bytes);
ssize_t circbuf_overwrite (circbuf_t *circ, const void *src, size_t bytes);
void   *circbuf_get_writeptr (circbuf_t *circ, size_t *size);
void   *circbuf_get_readptr (circbuf_t *circ, size_t *size);
void    circbuf_writecommit (circbuf_t *circ, size_t writtensize);
void    circbuf_readcommit (circbuf_t *circ, size_t readsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _CIRCBUF_H */
