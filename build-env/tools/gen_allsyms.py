#!/usr/bin/env python3
# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import sys
import os
import subprocess

HEADER = """/* auto-generated allsyms */
#include <symtab.h>
#define __systab    __attribute__((section(".systab")))
#define T(addr, name)   {(const char*)#name, (const void*)addr}

__systab const struct symtab_item g_allsyms[] =
{
"""

FOOTER = """};

__systab const int g_nallsyms = sizeof(g_allsyms) / sizeof(g_allsyms[0]);
"""

UNKNOWN_HEAD = '   T(0x0000000000000000, Unknown),\n'
UNKNOWN_TAIL = '   T(0x0000000000000000, Unknown),\n'


def write_empty(out_c):
    os.makedirs(os.path.dirname(out_c), exist_ok=True)
    with open(out_c, "w") as f:
        f.write(HEADER)
        f.write(UNKNOWN_HEAD)
        f.write(UNKNOWN_TAIL)
        f.write(FOOTER)

def gen_from_elf(out_c, elf):
    nm = os.environ.get("NM", "nm")

    try:
        result = subprocess.run(
            [nm, "-n", elf],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
    except Exception as e:
        print(f"[allsyms] nm failed: {e}", file=sys.stderr)
        write_empty(out_c)
        return

    symbols = []

    for line in result.stdout.splitlines():
        parts = line.strip().split()
        if len(parts) < 3:
            continue

        addr, typ, name = parts[:3]

        # Keep common text/data symbols plus weak symbols so module resolver
        # can see weak default implementations (e.g. __weak functions).
        if typ.lower() not in ("t", "d", "b", "r", "w", "v"):
            continue

        # 过滤无意义符号
        if name.startswith(".") or name.startswith("$"):
            continue

        symbols.append((addr, name))

    os.makedirs(os.path.dirname(out_c), exist_ok=True)
    with open(out_c, "w") as f:
        f.write(HEADER)
        f.write(UNKNOWN_HEAD)

        for addr, name in symbols:
            f.write(f'   T(0x{addr}, {name}),\n')

        f.write(UNKNOWN_TAIL)
        f.write(FOOTER)

def main():
    if len(sys.argv) < 2:
        print("Usage: gen_allsyms.py <allsyms.c> [elf]", file=sys.stderr)
        sys.exit(1)

    out_c = sys.argv[1]
    elf = sys.argv[2] if len(sys.argv) >= 3 else None

    # 如果 out_c 已经存在，先删除
    if os.path.exists(out_c):
        os.remove(out_c)

    if not elf or not os.path.isfile(elf):
        write_empty(out_c)
        return

    gen_from_elf(out_c, elf)

if __name__ == "__main__":
    main()
