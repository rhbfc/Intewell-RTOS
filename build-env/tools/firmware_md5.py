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
import subprocess
import tempfile
import os
import hashlib
import sys
import platform
import struct

def validate_toolchain_prefix(tool_prefix: str) -> bool:
    try:
        test_gcc = f"{tool_prefix}gcc"
        if platform.system() == "Windows" and not test_gcc.endswith(".exe"):
            test_gcc += ".exe"
        result = subprocess.run(
            [test_gcc, "-v"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        return result.returncode == 0
    except Exception:
        return False

def find_tool(tool_prefix: str, tool_name: str) -> str:
    full_path = f"{tool_prefix}{tool_name}"
    if platform.system() == "Windows":
        if not full_path.lower().endswith(".exe"):
            full_path += ".exe"
        if os.path.isfile(full_path):
            return full_path
        raise FileNotFoundError(f"[错误] 未找到工具：{full_path}")
    return full_path

def check_elf_file(path: str) -> bool:
    try:
        with open(path, "rb") as f:
            return f.read(4) == b"\x7fELF"
    except Exception:
        return False

def extract_section(tool_objcopy: str, section: str, elf_path: str) -> bytes:
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.close()
        subprocess.run([
            tool_objcopy,
            "-O", "binary",
            f"--only-section={section}",
            elf_path,
            tmp.name
        ], check=True)
        with open(tmp.name, "rb") as f:
            data = f.read()
        os.remove(tmp.name)
        return data

def md5_digest(data: bytes) -> bytes:
    return hashlib.md5(data).digest()

def create_md5_section(tool_objcopy: str, elf_path: str, text_md5: bytes, rodata_md5: bytes):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.write(text_md5 + rodata_md5)
        tmp.close()
        try:
            subprocess.run([
                tool_objcopy,
                "--update-section",
                f".md5_section={tmp.name}",
                elf_path
            ], check=True)
            print(f"[成功] 已将MD5结构体写入到.md5_section节")
        except subprocess.CalledProcessError as e:
            print(f"[错误] 更新.md5_section节失败: {e}")
        finally:
            os.remove(tmp.name)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="输入的 ELF 固件文件")
    parser.add_argument("--toolchain-prefix", required=False, default="", help="工具链前缀，如 aarch64-linux-gnu-")
    args = parser.parse_args()

    elf_path = args.input
    tool_prefix = args.toolchain_prefix

    print(f"[文件校验] 正在验证输入文件: {elf_path}")
    if not os.path.isfile(elf_path):
        print("[错误] 输入文件不存在")
        sys.exit(1)
    if not check_elf_file(elf_path):
        print("[错误] 输入文件不是合法的 ELF 格式")
        sys.exit(1)
    print("[文件校验] ELF文件验证通过")

    if tool_prefix:
        if not validate_toolchain_prefix(tool_prefix):
            print(f"[错误] 工具链无效：{tool_prefix}")
            sys.exit(1)
        try:
            objcopy = find_tool(tool_prefix, "objcopy")
            readelf = find_tool(tool_prefix, "readelf")
        except FileNotFoundError as e:
            print(str(e))
            sys.exit(1)
    else:
        objcopy = "objcopy"
        readelf = "readelf"
        print("[提示] 未设置工具链前缀，使用系统默认 objcopy/readelf")

    text_md5 = None
    rodata_md5 = None

    for section in [".text", ".rodata"]:
        try:
            data = extract_section(objcopy, section, elf_path)
            if not data:
                print(f"[跳过] 未提取 {section} 段")
                continue
            digest = md5_digest(data)
            print(f"[MD5校验] {section}段: {digest.hex()}")
            if section == ".text":
                text_md5 = digest
            elif section == ".rodata":
                rodata_md5 = digest
        except subprocess.CalledProcessError:
            print(f"[错误] objcopy 提取 {section} 段失败")
        except Exception as e:
            print(f"[错误] 提取 {section} 段出错: {e}")

    if text_md5 and rodata_md5:
        create_md5_section(objcopy, elf_path, text_md5, rodata_md5)
    else:
        print("[警告] 未获取完整的MD5信息，跳过写入.md5_section节")

if __name__ == "__main__":
    main()
