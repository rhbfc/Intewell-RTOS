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

#ifndef TTOS_VDSO_TEXT_FILL_SUFFIX
#define TTOS_VDSO_TEXT_FILL_SUFFIX
#endif

#ifndef TTOS_VDSO_DISCARD_EXTRA
#define TTOS_VDSO_DISCARD_EXTRA
#endif

#ifndef VDSO_CGT32_SYM
#define VDSO_CGT32_SYM
#endif

#define TTOS_VDSO_VERSION_NODE VDSO_CGT_VER

#define TTOS_VDSO_VERSION_GLOBALS VDSO_CGT_SYM; 

PROVIDE(__ttos_vdso_data = -4096); /* 把这个符号设为相对 vDSO 基址往前一页（4096 字节）的位置 */

SECTIONS
{
    . = SIZEOF_HEADERS;

    .hash : { *(.hash) } :text
    .gnu.hash : { *(.gnu.hash) } :text
    .dynsym : { *(.dynsym) } :text
    .dynstr : { *(.dynstr) } :text
    .gnu.version : { *(.gnu.version) } :text
    .gnu.version_d : { *(.gnu.version_d) } :text
    .gnu.version_r : { *(.gnu.version_r) } :text

    .dynamic : { *(.dynamic) } :text :dynamic

    .rodata : {
        *(.rodata*)
        *(.data*)
        *(.sdata*)
        *(.got.plt) *(.got)
        *(.bss*)
        *(.dynbss*)
    } :text

    .note : { *(.note.*) } :text :note

    .eh_frame_hdr : { *(.eh_frame_hdr) } :text :eh_frame_hdr
    .eh_frame : { *(.eh_frame) } :text

    .text : { *(.text*) } :text TTOS_VDSO_TEXT_FILL_SUFFIX

    /DISCARD/ : {
        TTOS_VDSO_DISCARD_EXTRA
        *(.note.gnu.property)
        *(.comment*)
        *(.discard*)
    }
}

#define PT_GNU_EH_FRAME 0x6474e550

PHDRS
{
    text PT_LOAD FLAGS(5) FILEHDR PHDRS;
    dynamic PT_DYNAMIC FLAGS(4);
    note PT_NOTE FLAGS(4);
    eh_frame_hdr PT_GNU_EH_FRAME;
}

VERSION
{
    TTOS_VDSO_VERSION_NODE
    {
        global:
            TTOS_VDSO_VERSION_GLOBALS
        local: *;
    };
}
