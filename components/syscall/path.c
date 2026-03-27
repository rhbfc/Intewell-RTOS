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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ttosProcess.h>

/**
 * @brief 简化路径。
 *
 * 该函数用于简化路径。
 *
 * @param path 路径。
 * @return 成功时返回简化后的路径，失败时返回 NULL。
 */
char *simplifyPath(const char *path)
{
    typedef struct stack
    {
        char character[PATH_MAX];
        int top;
    } stack;
    char *newpath;
    int i = 0;
    int dotcount = 0;
    stack *s1 = (stack *)malloc(sizeof(stack));
    if (s1 == NULL)
    {
        return NULL;
    }
    memset(s1, 0, sizeof(stack));
    if (path[0] == '\0')
    {
        free(s1);
        return NULL;
    }
    while (path[i] != '\0')
    {
        switch (path[i])
        {
        case '/':
            if (dotcount == 1 && s1->character[s1->top - 1] == '.')
            {
                dotcount = 0;
                s1->top -= 1;
                if (s1->character[s1->top - 1] != '/')
                {
                    s1->character[s1->top] = path[i];
                    s1->top++;
                    i++;
                    break;
                }
                else
                {
                    i++;
                    break;
                }
            }
            else if (dotcount == 2 && s1->character[s1->top - 1] == '.')
            {
                dotcount = 0;
                s1->top -= 2;

                if (s1->top == 1 && s1->character[s1->top - 1] == '/')
                {
                    i++;
                    break;
                }
                s1->top -= 1;
                while (s1->top > 0 && s1->character[s1->top - 1] != '/')
                {
                    s1->top--;
                }
                if (s1->character[s1->top - 1] != '/')
                {
                    s1->character[s1->top] = path[i];
                    s1->top++;
                    i++;
                    break;
                }
                else
                {
                    i++;
                    break;
                }
            }
            else
            {
                if (s1->top > 0)
                {
                    if (s1->character[s1->top - 1] == '/')
                    {
                        i++;
                        break;
                    }
                }
                s1->character[s1->top] = path[i];
                s1->top++;
                i++;
                break;
            }
        case '.':
            dotcount++;
            // i++;顺序错误
            s1->character[s1->top] = path[i];
            i++;
            s1->top++;
            break;
        default:
            dotcount = 0;
            s1->character[s1->top] = path[i];
            s1->top++;
            i++;
            break;
        }
    }

    if (s1->top > 1)
    {
        if (dotcount == 2 && s1->character[s1->top - 1] == '.')
        {
            dotcount = 0;
            s1->top -= 2;
            if (s1->top > 1 && s1->character[s1->top - 1] == '/')
            {
                s1->top -= 1;
                while (s1->top > 0 && s1->character[s1->top - 1] != '/')
                {
                    s1->top--;
                }
            }
        }
        if (dotcount == 1 && s1->character[s1->top - 1] == '.')
        {
            s1->top -= 1;
        }
        if (s1->top > 1 && s1->character[s1->top - 1] == '/')
        {
            s1->top--;
        }
    }
    newpath = (char *)malloc(sizeof(char) * (s1->top + 20)); // 多分配字节留给结束符
    if (newpath == NULL)
    {
        free(s1);
        return NULL;
    }
    strncpy(newpath, s1->character, s1->top); // 拷贝不带结束符的字符串
    newpath[s1->top] = '\0';                  // 手动添加结束符
    free(s1);
    return newpath;
}

/**
 * @brief 连接路径。
 *
 * 该函数用于连接两个路径。
 *
 * @param path 原路径。
 * @param join 要连接的路径。
 * @return 成功时返回连接后的路径，失败时返回 NULL。
 */
char *path_join(const char *path, const char *join)
{
    char *result;
    char *catpath;
    catpath = malloc(PATH_MAX);
    strncpy(catpath, path, PATH_MAX);
    strncat(catpath, "/", PATH_MAX);
    strncat(catpath, join, PATH_MAX);
    result = simplifyPath(catpath);
    free(catpath);
    return result;
}
char *mount_point_get_source_path(const char *path);

/**
 * @brief 获取进程的完整路径。
 *
 * 该函数用于获取进程的完整路径。
 *
 * @param dirfd 目录文件描述符。
 * @param path 文件路径。
 * @return 成功时返回完整路径，失败时返回 NULL。
 */
char *process_getfullpath(int dirfd, const char __user *path)
{
    pcb_t pcb = ttosProcessSelf();

    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    else if (path[0] == '/')
    {
        return simplifyPath(path);
    }

    if (dirfd == AT_FDCWD)
    {
        return path_join(pcb == NULL ? "/" : pcb->pwd, path);
    }
    else
    {
        int ret;
        char *realpath = NULL;
        char *fullpath = NULL;

        char *dirpath = malloc(PATH_MAX);

        dirpath[0] = '\0';

        ret = fcntl(dirfd, F_GETPATH, dirpath);
        if (ret >= 0)
        {
            fullpath = path_join(dirpath, path);
            realpath = mount_point_get_source_path(fullpath);
            free(fullpath);
        }

        free(dirpath);

        return realpath;
    }
}

/**
 * @brief 获取进程的完整路径。
 *
 * 该函数用于获取进程的完整路径。
 *
 * @param dirfd 目录文件描述符。
 * @param path 文件路径。
 * @return 成功时返回完整路径，失败时返回 NULL。
 */
char *process_getfilefullpath(struct file *f, const char __user *path)
{
    pcb_t pcb = ttosProcessSelf();

    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    else if (path[0] == '/')
    {
        return simplifyPath(path);
    }

    if (f == NULL)
    {
        return path_join(pcb == NULL ? "/" : pcb->pwd, path);
    }
    else
    {
        int ret;
        char *realpath = NULL;
        char *fullpath = NULL;

        char *dirpath = malloc(PATH_MAX);

        dirpath[0] = '\0';

        ret = file_fcntl(f, F_GETPATH, dirpath);
        if (ret >= 0)
        {
            fullpath = path_join(dirpath, path);
            realpath = mount_point_get_source_path(fullpath);
            free(fullpath);
        }

        free(dirpath);

        return realpath;
    }
}
