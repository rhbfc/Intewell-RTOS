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

import argparse
import shutil
import struct
import sys
from dataclasses import dataclass

from elftools.elf.elffile import ELFFile


ENTRY_SIZE = 12


@dataclass
class FaultRecoveryRecord:
    fault_addr: int
    resume_addr: int
    action: int
    data: int


def parse_args():
    parser = argparse.ArgumentParser(description="Sort __fault_recovery entries by fault address.")
    parser.add_argument("--input", required=True, help="Path to ELF image")
    parser.add_argument("--output", help="Path to output ELF image, defaults to in-place update")
    parser.add_argument("--section", default="__fault_recovery", help="Fault-recovery section name")
    return parser.parse_args()


def signed_rel_or_die(value: int, field_name: str) -> int:
    if -(1 << 31) <= value < (1 << 31):
        return value
    raise ValueError(f"{field_name} relative offset out of range: {value}")


def load_entries(elf_path: str, section_name: str):
    with open(elf_path, "rb") as elf_file:
        elf = ELFFile(elf_file)
        section = elf.get_section_by_name(section_name)
        if section is None:
            return None

        data = section.data()
        if len(data) % ENTRY_SIZE != 0:
            raise ValueError(f"{section_name} size {len(data)} is not a multiple of {ENTRY_SIZE}")

        fmt = ("<" if elf.little_endian else ">") + "iiHH"
        entries = []
        for offset in range(0, len(data), ENTRY_SIZE):
            fault_rel, resume_rel, action, extra = struct.unpack(fmt, data[offset:offset + ENTRY_SIZE])
            entry_addr = section["sh_addr"] + offset
            entries.append(
                FaultRecoveryRecord(
                    fault_addr=entry_addr + fault_rel,
                    resume_addr=entry_addr + 4 + resume_rel,
                    action=action,
                    data=extra,
                )
            )

        return {
            "fmt": fmt,
            "offset": section["sh_offset"],
            "addr": section["sh_addr"],
            "entries": entries,
        }


def build_sorted_blob(section_addr: int, fmt: str, entries):
    ordered = sorted(entries, key=lambda entry: entry.fault_addr)
    blob = bytearray()

    for index, entry in enumerate(ordered):
        entry_addr = section_addr + index * ENTRY_SIZE
        fault_rel = signed_rel_or_die(entry.fault_addr - entry_addr, "fault")
        resume_rel = signed_rel_or_die(entry.resume_addr - (entry_addr + 4), "resume")
        blob.extend(struct.pack(fmt, fault_rel, resume_rel, entry.action, entry.data))

    return bytes(blob)


def main():
    args = parse_args()
    output_path = args.output or args.input

    if output_path != args.input:
        shutil.copyfile(args.input, output_path)

    section_info = load_entries(output_path, args.section)
    if section_info is None:
        print(f"[fault-recovery] section '{args.section}' not found, skip")
        return 0

    original = section_info["entries"]
    if not original:
        print(f"[fault-recovery] section '{args.section}' empty, skip")
        return 0

    sorted_blob = build_sorted_blob(section_info["addr"], section_info["fmt"], original)

    with open(output_path, "r+b") as elf_file:
        elf_file.seek(section_info["offset"])
        current_blob = elf_file.read(len(sorted_blob))
        if current_blob == sorted_blob:
            print(f"[fault-recovery] section '{args.section}' already sorted ({len(original)} entries)")
            return 0

        elf_file.seek(section_info["offset"])
        elf_file.write(sorted_blob)

    print(f"[fault-recovery] sorted section '{args.section}' ({len(original)} entries)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"[fault-recovery] error: {exc}", file=sys.stderr)
        sys.exit(1)
