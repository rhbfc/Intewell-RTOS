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

#include "syscall_internal.h"
#include <assert.h>
#include <errno.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 扩展文件状态结构体
 * 
 * 该结构体提供了比传统stat结构体更详细的文件状态信息。
 */
struct statx {
    uint32_t stx_mask;          /**< 请求的信息掩码 */
    uint32_t stx_blksize;       /**< 块大小 */
    uint64_t stx_attributes;     /**< 文件属性 */
    uint32_t stx_nlink;         /**< 硬链接数 */
    uint32_t stx_uid;           /**< 用户ID */
    uint32_t stx_gid;           /**< 组ID */
    uint16_t stx_mode;          /**< 文件模式 */
    uint16_t pad1;              /**< 填充字段 */
    uint64_t stx_ino;           /**< inode号 */
    uint64_t stx_size;          /**< 文件大小 */
    uint64_t stx_blocks;        /**< 已分配块数 */
    uint64_t stx_attributes_mask;/**< 支持的属性掩码 */
    struct {
        int64_t  tv_sec;        /**< 秒数 */
        uint32_t tv_nsec;       /**< 纳秒数 */
        int32_t  pad;           /**< 填充字段 */
    } stx_atime,                /**< 访问时间 */
      stx_btime,                /**< 创建时间 */
      stx_ctime,                /**< 状态改变时间 */
      stx_mtime;                /**< 修改时间 */
    uint32_t stx_rdev_major;    /**< 设备号主编号 */
    uint32_t stx_rdev_minor;    /**< 设备号次编号 */
    uint32_t stx_dev_major;     /**< 所在设备主编号 */
    uint32_t stx_dev_minor;     /**< 所在设备次编号 */
    uint64_t spare[14];         /**< 保留字段 */
};

/**
 * @brief 系统调用实现：获取文件状态信息（扩展版）。
 *
 * 该函数实现了一个系统调用，用于获取文件的详细状态信息，提供了比传统stat更多的信息。
 *
 * @param[in] dfd 目录文件描述符：
 *                - AT_FDCWD：使用当前工作目录
 *                - 其他值：使用指定目录
 * @param[in] path 目标文件路径
 * @param[in] flags 标志位：
 *                  - AT_SYMLINK_NOFOLLOW：不跟随符号链接
 *                  - AT_NO_AUTOMOUNT：不自动挂载
 * @param[in] mask 请求的信息掩码
 * @param[out] buffer statx结构体缓冲区
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -ENAMETOOLONG 文件名过长。
 * @retval -ENOENT 文件不存在。
 * @retval -EACCES 权限不足。
 *
 * @note 1. 支持相对路径。
 *       2. 支持符号链接控制。
 *       3. 提供纳秒级时间戳。
 *       4. 提供扩展文件属性。
 */
DEFINE_SYSCALL (statx, (int dfd, const char __user *path, unsigned flags,
                        unsigned mask, void __user *buffer))
{
    int         ret = 0;
    struct stat stbuf;
    pcb_t       pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    struct statx kstx;

    if( (path != NULL) && (strlen_user(path) >= PATH_MAX) )
    {
        return -ENAMETOOLONG;   /* 文件路径太长 */
    }

    if (!user_access_check (buffer, sizeof (struct statx), UACCESS_W))
    {
        return -EINVAL;
    }

    char *fullpath = process_getfullpath (dfd, path);
    if (!fullpath)
    {
        return -1;
    }
    ret = access (fullpath, F_OK);
    if (ret != 0)
    {
        free (fullpath);
        return ret;
    }

    ret = vfs_stat (fullpath, &stbuf, (AT_SYMLINK_NOFOLLOW & flags) ? 0 : 1);

    free (fullpath);

    if (ret == 0)
    {
        kstx = (struct statx){ .stx_dev_major     = major (stbuf.st_dev),
                               .stx_dev_minor     = minor (stbuf.st_dev),
                               .stx_rdev_major    = major (stbuf.st_rdev),
                               .stx_rdev_minor    = minor (stbuf.st_rdev),
                               .stx_ino           = stbuf.st_ino,
                               .stx_mode          = stbuf.st_mode,
                               .stx_nlink         = stbuf.st_nlink,
                               .stx_uid           = stbuf.st_uid,
                               .stx_gid           = stbuf.st_gid,
                               .stx_size          = stbuf.st_size,
                               .stx_blksize       = stbuf.st_blksize,
                               .stx_blocks        = stbuf.st_blocks,
                               .stx_atime.tv_sec  = stbuf.st_atim.tv_sec,
                               .stx_atime.tv_nsec = stbuf.st_atim.tv_nsec,
                               .stx_mtime.tv_sec  = stbuf.st_mtim.tv_sec,
                               .stx_mtime.tv_nsec = stbuf.st_mtim.tv_nsec,
                               .stx_ctime.tv_sec  = stbuf.st_ctim.tv_sec,
                               .stx_ctime.tv_nsec = stbuf.st_ctim.tv_nsec };

        copy_to_user (buffer, &kstx, sizeof (kstx));
    }

    return ret;
}