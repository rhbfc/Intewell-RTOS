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

#include <git_version.h>
#include <git_register.h>
#include <stdio.h>
#include <string.h>
#include <shell.h>

#define OS_GIT_INFO_MAX  32   /* 最多支持多少个库 */

struct os_git_info {
    const char *repo;
    const char *hash;
    const char *branch;
};


static struct os_git_info g_git_infos[OS_GIT_INFO_MAX];
static unsigned int g_git_info_cnt = 0;

void os_register_git_info(const char *repo,
                          const char *hash,
                          const char *branch)
{
    if (!repo || !hash || !branch)
        return;

    if (g_git_info_cnt >= OS_GIT_INFO_MAX)
        return; /* 或者 assert / 打印警告 */

    g_git_infos[g_git_info_cnt].repo = repo;
    g_git_infos[g_git_info_cnt].hash = hash;
    g_git_infos[g_git_info_cnt].branch = branch;

    g_git_info_cnt++;
}

static void os_dump_git_info(void)
{
    unsigned int i;

    size_t w_repo = strlen("repo");
    size_t w_hash = strlen("hash");
    size_t w_branch = strlen("branch");

    /* 1. 计算每一列的最大宽度 */
    for (i = 0; i < g_git_info_cnt; i++) {
        if (g_git_infos[i].repo)
            if (strlen(g_git_infos[i].repo) > w_repo)
                w_repo = strlen(g_git_infos[i].repo);

        if (g_git_infos[i].hash)
            if (strlen(g_git_infos[i].hash) > w_hash)
                w_hash = strlen(g_git_infos[i].hash);

        if (g_git_infos[i].branch)
            if (strlen(g_git_infos[i].branch) > w_branch)
                w_branch = strlen(g_git_infos[i].branch);
    }

    /* 2. 打印表头 */
    printf("Git Info (%u entries)\n", g_git_info_cnt);
    printf("------------------------------------------------------------\n");
    printf("%-3s  %-*s  %-*s  %-*s\n",
           "#",
           (int) w_repo, "repo",
           (int) w_hash, "hash",
           (int) w_branch, "branch");

    printf("%-3s  %-*s  %-*s  %-*s\n",
           "---",
           (int) w_repo, "----",
           (int) w_hash, "----",
           (int) w_branch, "------");

    /* 3. 打印内容 */
    for (i = 0; i < g_git_info_cnt; i++) {
        printf("%-3u  %-*s  %-*s  %-*s\n",
               i,
               (int) w_repo, g_git_infos[i].repo ? g_git_infos[i].repo : "-",
               (int) w_hash, g_git_infos[i].hash ? g_git_infos[i].hash : "-",
               (int) w_branch, g_git_infos[i].branch ? g_git_infos[i].branch : "-");
    }

    printf("------------------------------------------------------------\n");
}


REGISTER_GIT_INFO(os);

static int version_cmd(size_t argc, char **argv)
{
    os_dump_git_info();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)
                | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)
                | SHELL_CMD_DISABLE_RETURN,
                version, version_cmd, show version info);