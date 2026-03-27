# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include(${CMAKE_SOURCE_DIR}/build-env/cmake/vdso_common.cmake)

set(VDSO_CFLAGS
    -fPIC
    -O2
    -fasynchronous-unwind-tables
    -fno-stack-protector
    -fno-omit-frame-pointer
    -foptimize-sibling-calls
    -DDISABLE_BRANCH_PROFILING
    -DBUILD_VDSO
)

set(VDSO_LINK_FLAGS
    -nostdlib
    -shared
    -Wl,--hash-style=both
    -Wl,--build-id=sha1
    -Wl,--no-undefined
    -Wl,--eh-frame-hdr
    -Wl,-Bsymbolic
    -Wl,-z,noexecstack
    -Wl,-z,max-page-size=4096
    -Wl,-T,${VDSO_LDS}
)

set(VDSO_ASFLAGS)
os_vdso_build()
