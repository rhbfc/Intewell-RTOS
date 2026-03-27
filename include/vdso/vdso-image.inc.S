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

#ifndef TTOS_VDSO_SO_FILE
#define TTOS_VDSO_SO_FILE "vdso.so"
#endif

#ifndef TTOS_VDSO_IMAGE_PROGBITS
#define TTOS_VDSO_IMAGE_PROGBITS %progbits
#endif

#ifndef TTOS_VDSO_IMAGE_OBJECT_TYPE
#define TTOS_VDSO_IMAGE_OBJECT_TYPE %object
#endif

#ifndef TTOS_VDSO_IMAGE_ADDR_WORD
#define TTOS_VDSO_IMAGE_ADDR_WORD .quad
#endif

.section .rodata.vdso.image, "a", TTOS_VDSO_IMAGE_PROGBITS
    .balign 4096
    .global __ttos_vdso_image_start
    .type __ttos_vdso_image_start, TTOS_VDSO_IMAGE_OBJECT_TYPE
__ttos_vdso_image_start:
    .incbin TTOS_VDSO_SO_FILE
    .global __ttos_vdso_image_end
    .type __ttos_vdso_image_end, TTOS_VDSO_IMAGE_OBJECT_TYPE
__ttos_vdso_image_end:
    .balign 4096

    .global __ttos_vdso_image_vaddr
    .type __ttos_vdso_image_vaddr, TTOS_VDSO_IMAGE_OBJECT_TYPE
__ttos_vdso_image_vaddr:
    TTOS_VDSO_IMAGE_ADDR_WORD __ttos_vdso_image_start

    /*
     * Dedicated vvar page mapped into user space alongside vDSO.
     * Keep it page-aligned and page-sized to avoid sharing unrelated kernel data.
     */
.section .data.vdso.vvar, "aw", TTOS_VDSO_IMAGE_PROGBITS
    .balign 4096
    .global __ttos_vdso_vvar_start
    .type __ttos_vdso_vvar_start, TTOS_VDSO_IMAGE_OBJECT_TYPE
__ttos_vdso_vvar_start:
    .zero 4096
    .global __ttos_vdso_vvar_end
    .type __ttos_vdso_vvar_end, TTOS_VDSO_IMAGE_OBJECT_TYPE
__ttos_vdso_vvar_end:

.section .note.GNU-stack, "", TTOS_VDSO_IMAGE_PROGBITS
