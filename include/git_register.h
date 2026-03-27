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

/* git_register.h */
#include <version.h>
#include <ttos_init.h>

/* 默认值 */
#define _GIT_REPO_DEFAULT   "unknown"
#define _GIT_HASH_DEFAULT   "unknown"
#define _GIT_BRANCH_DEFAULT "unknown"

/* 判断宏是否存在并取值 */
#ifdef GIT_REPO
#  define _GIT_REPO_VALUE GIT_REPO
#else
#  define _GIT_REPO_VALUE _GIT_REPO_DEFAULT
#endif

#ifdef GIT_HASH
#  define _GIT_HASH_VALUE GIT_HASH
#else
#  define _GIT_HASH_VALUE _GIT_HASH_DEFAULT
#endif

#ifdef GIT_BRANCH
#  define _GIT_BRANCH_VALUE GIT_BRANCH
#else
#  define _GIT_BRANCH_VALUE _GIT_BRANCH_DEFAULT
#endif

#define _CAT(a, b) a##b
#define CAT(a, b)  _CAT(a, b)

#define REGISTER_GIT_INFO_CALL()                                    \
    os_register_git_info(                                           \
        _GIT_REPO_VALUE,                                            \
        _GIT_HASH_VALUE,                                            \
        _GIT_BRANCH_VALUE                                           \
    )


#define REGISTER_GIT_INFO_IMPL(tag)                                 \
static int CAT(git_info_component_init_, tag)(void)                 \
{                                                                   \
    REGISTER_GIT_INFO_CALL();                                       \
    return 0;                                                       \
}                                                                   \
INIT_EXPORT_COMPONENTS(                                             \
    CAT(git_info_component_init_, tag),                             \
    "git info register"                                             \
)


#define REGISTER_GIT_INFO(tag)     REGISTER_GIT_INFO_IMPL(tag)