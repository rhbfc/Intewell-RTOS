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

#ifndef __VERSION_H__
#define __VERSION_H__


#define INTEWELL_VERSION_MAJOR 3
#define INTEWELL_VERSION_MINOR 1
#define INTEWELL_VERSION_RELEASE 1

#define __QUOTE(str) #str
#define __EXPAND_AND_QUOTE(str) __QUOTE(str)
#define __VERSION_NUM INTEWELL_VERSION_MAJOR.INTEWELL_VERSION_MINOR.INTEWELL_VERSION_RELEASE

#if defined(__has_include)
#if __has_include(<git_version.h>)
#include <git_version.h>
#endif
#endif

#ifndef VERSION_TAG
#define VERSION_TAG
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

#ifndef GIT_REVISION
#define GIT_REVISION GIT_HASH
#endif

#define INTEWELL_VERSION_STRING __EXPAND_AND_QUOTE(__VERSION_NUM) VERSION_TAG
#define GIT_INFO GIT_HASH

#define VERSION(maj, min, mic) (maj << 24 | min << 16 | mic << 8)

#define INTEWELL_VERSION                                                                           \
    VERSION(INTEWELL_VERSION_MAJOR, INTEWELL_VERSION_MINOR, INTEWELL_VERSION_RELEASE)

void os_register_git_info(const char *repo,
                          const char *hash,
                          const char *branch);

#endif
