# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import sys
import re
import subprocess

def run(cmd, env):
    print(">>>", " ".join(cmd), flush=True)
    ret = subprocess.call(cmd, env=env)
    if ret != 0:
        sys.exit(ret)

def _c_escape_string(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\"", "\\\"")

def _format_c_value(v: str) -> tuple[str | None, bool]:
    """
    return: (formatted_value_or_None, is_undef)
      - formatted_value_or_None: 用于 #define 的值
      - is_undef: True 表示生成 /* #undef KEY */
    """
    v = (v or "").strip()
    if v == "":
        # 空值：按字符串空处理（你也可以改成 undef）
        return "\"\"", False

    low = v.lower()
    if low in ("y", "yes", "true", "1"):
        return "1", False
    if low in ("n", "no", "false", "0"):
        return None, True
    if low == "m":
        # Kconfig module 语义：这里先当作启用
        return "1", False

    # 已经是带引号字符串
    if (len(v) >= 2) and ((v[0] == v[-1] == "\"") or (v[0] == v[-1] == "'")):
        if v[0] == "'":
            # 单引号统一转成双引号（可选）
            inner = v[1:-1]
            return f"\"{_c_escape_string(inner)}\"", False
        return v, False

    # 纯数字（含负数）
    if re.fullmatch(r"-?\d+", v):
        return v, False

    # 其他都当字符串：自动加引号
    return f"\"{_c_escape_string(v)}\"", False


def write_allconfig(build_dir: str, extra: str) -> str | None:
    """
    将 EXTRA_KCONFIG_OPTIONS 直接生成 autoconfig_yaml.h

    extra 每行一个赋值，形如：
      CONFIG_FOO=y
      CONFIG_BOARD_PATH=external/example/...
      CONFIG_NAME="already quoted"
    """
    extra = (extra or "").strip()
    if not extra:
        return None

    out_path = os.path.join(build_dir, "autoconfig_yaml.h")

    lines_out: list[str] = []
    lines_out.append("/* Auto generated from EXTRA_KCONFIG_OPTIONS */")
    lines_out.append("#include \"autoconfig.h\"")
    lines_out.append("")

    defined_any = False

    for line in extra.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue

        k, v = line.split("=", 1)
        k = k.strip()
        v = v.strip()
        if not k:
            continue

        # 可选：强制只接受 CONFIG_ 开头（不想限制就删掉这段）
        # if not k.startswith("CONFIG_"):
        #     continue

        value, is_undef = _format_c_value(v)
        if is_undef:
            lines_out.append(f"/* #undef {k} */")
        else:
            lines_out.append(f"#define {k} {value}")
        defined_any = True

    if not defined_any:
        return None

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_out) + "\n")

    return out_path



def main():
    # 从命令行接收参数（避免写死）
    kconfig      = sys.argv[1]
    defconfig    = sys.argv[2]
    autoconfig_h = sys.argv[3]
    build_dir    = sys.argv[4]

    env = os.environ.copy()
    env["KCONFIG_BASE"] = build_dir
    env["srctree"] = os.environ.get("SRCTREE", "")
    env["KCONFIG_CONFIG"] = os.path.join(build_dir, ".config")

    # ★ 关键：把 CMake 传来的 EXTRA_KCONFIG_OPTIONS 写成 allconfig，并启用 KCONFIG_ALLCONFIG
    write_allconfig(build_dir, env.get("EXTRA_KCONFIG_OPTIONS", ""))

    # 1. defconfig
    run([
        sys.executable,
        os.path.join(os.path.dirname(__file__), "Kconfiglib", "defconfig.py"),
        "--kconfig", kconfig,
        defconfig,
    ], env)

    # 2. genconfig
    run([
        sys.executable,
        os.path.join(os.path.dirname(__file__), "Kconfiglib", "genconfig.py"),
        kconfig,
        "--header-path", autoconfig_h,
    ], env)

if __name__ == "__main__":
    main()
