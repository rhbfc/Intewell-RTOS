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
 * @file: configs.c
 * @brief:
 *    <li> 系统配置 </li>
 */

/************************头 文 件******************************/
#include <fs/fs.h>
#include <stddef.h>
#include <ttos_init.h>

#define KLOG_TAG "board"
#include <klog.h>
/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/

/************************函数实现******************************/

static int board_rootfs_init(void)
{
    int ret = 0;
#ifdef CONFIG_ROOTFS_EXT4
    ret = vfs_mount("/dev/virtblk0", "/", "ext4", 0, "autoformat");
    if (ret < 0)
    {
        KLOG_E("ERROR: Failed to mount lwext4 at %s: %d", "/", ret);
        return ret;
    }
#else /* CONFIG_ROOTFS_CPIO */
    ret = vfs_mount(NULL, "/", "ramfs", 0, "autoformat");
    if (ret < 0)
    {
        KLOG_E("ERROR: Failed to mount ramfs %d", ret);
        return ret;
    }
#endif
    ret = vfs_mount(NULL, "/run", "tmpfs", 0, "autoformat");
    if (ret < 0)
    {
        KLOG_E("ERROR: Failed to mount ramfs %d", ret);
        return ret;
    }

    return ret;
}
INIT_EXPORT_ROOT_FS(board_rootfs_init, "rootfs init");
