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
#include <commonTypes.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <system/compiler.h>
#include <ttos_init.h>

#include <initrd.h>
#include <system/macros.h>

#undef KLOG_TAG
#define KLOG_TAG "initramfs"
#include <klog.h>

static unsigned long simple_strtoul_hex(char *str)
{
    unsigned long result = 0;
    char c;
    int i = 0;

    while ((c = str[i]) != '\0' && i < 8)
    {                 // 最多处理8位16进制数
        result <<= 4; // 相当于乘以16

        if (c >= '0' && c <= '9')
        {
            result |= (c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            result |= (c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            result |= (c - 'A' + 10);
        }
        else
        {
            break; // 遇到非16进制字符就停止
        }
        i++;
    }

    return result;
}

#include <errno.h>
static int buf = 0;

static int do_unpack_cpio(const char *cpio_data, unsigned long cpio_size)
{
    const char *p = cpio_data;
    const char *end = cpio_data + cpio_size;

    struct cpio_header *header, *next_header;
    unsigned long mode, filesize, namesize, perm, symlink_mode;
    char *filename, *target;

    int ret = 0;

    while (p < end)
    {
        header = (struct cpio_header *)p;

        /* 检查cpio magic字段 */
        if (memcmp(header->magic, "070701", 6) != 0 && memcmp(header->magic, "070702", 6) != 0)
        {
            if (memcmp(header->magic, "070707", 6) == 0)
                KLOG_EMERG("incorrect cpio method used: use -H newc option");
            else
                KLOG_EMERG("no cpio magic, value %.6s", header->magic);

            return -1;
        }

        p += sizeof(struct cpio_header);

        mode = simple_strtoul_hex(header->mode);
        filesize = simple_strtoul_hex(header->filesize);
        namesize = simple_strtoul_hex(header->namesize);
        filename = strndup(p, namesize);

        /* 获取权限 */
        perm = mode & 0777;

        /* 文件名后进行 4 字节对齐 */
        p += namesize;
        p = (char *)ALIGN_UP((uintptr_t)p, 4);

        /* 不处理 . 和 .. */
        if (strcmp(filename, ".") == 0)
        {
            continue;
        }

        // extern int printk(const char *, ...);
        // printk("%s, mode %lu size %d, type: %s, open perm %04o\n", filename, mode, filesize,
        //        S_ISDIR(mode) ? "dir" : (S_ISREG(mode) ? "file" : (S_ISLNK(mode) ? "link" : "else")),
        //        mode & 0777);

        /* 特殊处理cpio的最后一个文件名 */
        if (strcmp(filename, "TRAILER!!!") == 0)
        {
            free(filename);
            ret = 0;
            break;
        }

        // 根据类型处理文件
        if (S_ISDIR(mode))
        {
            // 创建目录
            ret = vfs_mkdir(filename, perm);
            if (ret)
            {
                if (memcmp(filename, "dev", 3) == 0)
                {
                    KLOG_E("----> todo: dev/ already exist, we handle this later");
                }
                else
                {
                    KLOG_E("mkdir %s failed, ret %d", filename, ret);
                    goto out;
                }
            }
        }
        else if (S_ISREG(mode))
        {
            // 创建普通文件并写入数据
            struct file file = {0};
            ret = file_open(&file, filename, O_CREAT | O_WRONLY, perm);
            if (!ret && file.f_inode != NULL)
            {
                file_write(&file, p, filesize);
                file_close(&file);
            }
            else
            {
                KLOG_E("file %s open failed, ret %d", filename, ret);
                goto out;
            }
        }
        else if (S_ISLNK(mode))
        {
            target = strndup(p, filesize);
            // 创建符号链接
            ret = vfs_symlink(target, filename);
            if (ret)
            {
                KLOG_E("make symlink %s (target: %s) failed, ret %d", filename, target, ret);
                free(target);
                goto out;
            }
            free(target);
        }
        else
        {
            // 跳过其他类型（如设备文件）
        }

        p += filesize;
        /* 将header、namesize、filesize整体按照4字节对齐，推算下一个header的位置 */
        p = (char *)ALIGN_UP((uintptr_t)p, 4);

        free(filename);
    }

out:
    return ret;
}

static int __inittext unpack_cpio_rootfs(void)
{
    int ret = 0;

    ret = do_unpack_cpio(__initramfs_start, __initramfs_size);
    if (ret)
    {
        KLOG_EMERG("unpack rootfs failed, while 1 at %s:%d", __FILE__, __LINE__);

        while (1)
            ;
        /* todo 失败的其他处理 */
    }
}

INIT_EXPORT_INITRD(unpack_cpio_rootfs, "cpio rootfs unpack and init");