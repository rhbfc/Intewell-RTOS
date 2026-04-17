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

import os
import re
import sys
import yaml
import json
import shutil
import argparse
from pathlib import Path


# ============================================================
# 路径 & 变量定义
# ============================================================

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

VARS = {
    "YAML_ROOT": os.path.abspath(
        os.path.join(SCRIPT_DIR, "../yaml")
    ),
}
EXT_BOARD_DIT_NAME = "boards-prj"

# ============================================================
# 基础工具函数
# ============================================================

def deep_merge(base, override):
    """
    深度合并 dict，override 覆盖 base
    """
    for k, v in override.items():
        if (
            k in base
            and isinstance(base[k], dict)
            and isinstance(v, dict)
        ):
            deep_merge(base[k], v)
        else:
            base[k] = v
    return base


_VAR_PATTERN = re.compile(r"\$\{([^}]+)\}")

def expand_vars(value: str, vars: dict):
    """
    展开字符串中的变量：
      - ${YAML_ROOT}
      - ${ENV:XXX}
    """
    if not isinstance(value, str):
        return value

    def repl(match):
        key = match.group(1)

        # 环境变量
        if key.startswith("ENV:"):
            env_key = key[4:]
            if env_key not in os.environ:
                raise ValueError(f"[ERROR] 环境变量未定义: {env_key}")
            return os.environ[env_key]

        # 普通变量
        if key not in vars:
            raise ValueError(f"[ERROR] 未定义变量: {key}")

        return str(vars[key])

    return _VAR_PATTERN.sub(repl, value)


def expand_data(value, vars):
    if isinstance(value, str):
        return expand_vars(value, vars)
    if isinstance(value, list):
        return [expand_data(item, vars) for item in value]
    if isinstance(value, dict):
        return {
            key: expand_data(item, vars)
            for key, item in value.items()
        }
    return value


def resolve_vars(base_vars, extra_vars, source_file):
    merged = dict(base_vars)
    pending = dict(extra_vars)

    while pending:
        progressed = False

        for key, value in list(pending.items()):
            if not isinstance(value, str):
                merged[key] = value
                pending.pop(key)
                progressed = True
                continue

            try:
                merged[key] = expand_vars(value, merged)
            except ValueError:
                continue

            pending.pop(key)
            progressed = True

        if not progressed:
            unresolved = ", ".join(sorted(pending.keys()))
            raise ValueError(
                f"[ERROR] {source_file}: vars 中存在未解析变量: {unresolved}"
            )

    return merged


# ============================================================
# YAML 加载（支持 include + 变量 + 循环检测）
# ============================================================

def load_yaml(path, visited=None, vars=None):
    if visited is None:
        visited = set()
    if vars is None:
        vars = VARS

    path = os.path.abspath(path)

    if path in visited:
        raise RuntimeError(f"[ERROR] 循环 include: {path}")
    visited.add(path)

    with open(path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    if not isinstance(data, dict):
        raise ValueError(f"[ERROR] {path}: YAML 顶层必须是 dict")

    result = {}

    includes = data.get("include", [])
    if not isinstance(includes, list):
        raise ValueError(f"[ERROR] {path}: include 必须是 list")

    for inc in includes:
        inc_path = expand_vars(inc, vars)

        if not os.path.isabs(inc_path):
            inc_path = os.path.join(os.path.dirname(path), inc_path)

        inc_data = load_yaml(inc_path, visited, vars)
        deep_merge(result, inc_data)

    data_no_include = dict(data)
    data_no_include.pop("include", None)
    deep_merge(result, data_no_include)

    extra_vars = result.get("vars", {})
    if extra_vars is None:
        extra_vars = {}
    if not isinstance(extra_vars, dict):
        raise ValueError(f"[ERROR] {path}: vars 必须是 dict")

    merged_vars = resolve_vars(vars, extra_vars, path)

    return expand_data(result, merged_vars)

def get_by_path(data, path):
    cur = data
    for p in path.split("."):
        if not isinstance(cur, dict) or p not in cur:
            return None
        cur = cur[p]
    return cur

def set_by_path(data, path, value):
    keys = path.split(".")
    cur = data
    for k in keys[:-1]:
        cur = cur.setdefault(k, {})
    cur[keys[-1]] = value

def apply_schema(schema, data, source_file):
    preset = {}
    fields = schema["preset"]["fields"]
    for req in schema["preset"].get("required", []):
        if get_by_path(data, req) is None:
            raise ValueError(f"[ERROR] {source_file}: 缺少必填字段 '{req}'")
    for path, rule in fields.items():
        value = get_by_path(data, path)
        if value is None:
            continue
        if rule["type"] == "string":
            set_by_path(preset, rule["map_to"], value)
        elif rule["type"] == "map":
            for k, v in value.items():
                target = rule["map_each_to"].replace("*", k)
                set_by_path(preset, target, v)
        elif rule["type"] == "list":
            pass
    return preset


def get_ext_board_prefix(fpath):
    """
    如果是 external board，返回 external 包名
    否则返回 None
    """
    p = Path(fpath).resolve()

    parts = p.parts
    if EXT_BOARD_DIT_NAME not in parts:
        return None

    idx = parts.index(EXT_BOARD_DIT_NAME)

    # 期望结构： boards-prj/<pkg>/boards/*.yaml
    if len(parts) > idx + 3 and parts[idx + 2] == "yaml":
        return parts[idx + 1]

    return None

# ============================================================
# 主入口
# ============================================================
def main():
    parser = argparse.ArgumentParser(
        prog="tbox",
        description="Generate CMakePresets.json from board yamls"
    )
    parser.add_argument("schema", help="schema.yaml")
    parser.add_argument(
        "board_dirs",
        nargs="*",
        help="board directories"
    )
    parser.add_argument(
        "-f", "--file",
        help="只生成指定的 board yaml（文件名或路径）"
    )
    args = parser.parse_args()

    schema = load_yaml(args.schema)
    presets = []
    preset_names = set()

    # ----------- 扫描 yaml 文件 -----------
    yaml_files = []
    if args.file:
        if os.path.isfile(args.file):
            yaml_files.append(os.path.abspath(args.file))
        else:
            found = False
            for board_dir in args.board_dirs:
                cand = os.path.join(board_dir, args.file)
                if os.path.isfile(cand):
                    yaml_files.append(os.path.abspath(cand))
                    found = True
                    break
            if not found:
                raise RuntimeError(f"[ERROR] 未找到指定文件: {args.file}")
    else:
        for board_dir in args.board_dirs:
            if not os.path.isdir(board_dir):
                print(f"[WARN] skip non-directory: {board_dir}")
                continue
            for fname in sorted(os.listdir(board_dir)):
                if fname.endswith(".yaml"):
                    yaml_files.append(
                        os.path.abspath(os.path.join(board_dir, fname))
                    )

    # ----------- 生成 preset -----------
    for fpath in yaml_files:
        cfg = load_yaml(fpath)
        print(f"[preset] load {fpath}")

        preset = apply_schema(schema, cfg, fpath)
        preset["generator"] = "Ninja" if shutil.which("ninja") else "Unix Makefiles"

        rel = Path(os.path.relpath(fpath))
        cache_vars = preset.setdefault("cacheVariables", {})
        cache_vars["BOARD_YAML"] = "./" + str(rel)
        cache_vars["BOARD_PATH"] = str(rel.parent.parent)
        cache_vars["CONFIG_FILE"] = str(rel.parent.parent) + f"/configs/{cache_vars['CONFIG_FILE']}"

        dtb = cache_vars.get('DTB_FILE')
        board_path = cache_vars.get('BOARD_PATH')

        if dtb:
            if not os.path.isabs(dtb):
                dtb = os.path.join(board_path, "dtb", dtb)

            cache_vars['CONFIG_DTB_PATH'] = os.path.abspath(dtb)


        preset_name = preset.get("name")
        if not preset_name:
            raise RuntimeError(f"[ERROR] preset missing 'name' field ({fpath})")

        # --------------------------------------------------
        # external board 名称前缀处理
        # --------------------------------------------------
        ext_prefix = get_ext_board_prefix(fpath)
        if ext_prefix:
            preset_name = f"{ext_prefix}-{preset_name}"
            preset["name"] = preset_name
        cache_vars["CMAKE_PRESET_NAME"] = preset_name

        rel = os.path.relpath(fpath)
        p = Path(os.path.splitext(rel)[0])
        parts = list(p.parts)
        parts[parts.index("yaml")] = "boards"
        parts[-1] = cache_vars['CONFIG_BOARD']

        cache_vars["CONFIG_BOARD_PATH"] = str(Path(*parts))

        if preset_name in preset_names:
            raise RuntimeError(
                f"[ERROR] 重复的 preset 名称: '{preset_name}' ({fpath})"
            )

        preset["binaryDir"] = preset.get("binaryDir", f"build/{cache_vars['CONFIG_SOFT_ARCH']}/{preset_name}")
        preset.setdefault("environment", {})
        preset["environment"]["UPBOARD_DEPS_SYNC_REMOTE"] = "1"

        preset_names.add(preset_name)
        presets.append(preset)

    # ----------- 写入 CMakePresets.json -----------
    configure_presets = presets
    build_presets = []
    for p in presets:
        build_presets.append({
            "name": p["name"],
            "configurePreset": p["name"],
            "environment": {
                "UPBOARD_DEPS_SYNC_REMOTE": "0"
            }
        })

    cmake_presets = {
        "version": 3,
        "configurePresets": configure_presets,
        "buildPresets": build_presets
    }

    with open("CMakePresets.json", "w", encoding="utf-8") as f:
        json.dump(cmake_presets, f, indent=2, ensure_ascii=False)

    print("\n[OK] CMakePresets.json generated")
    print(f"[OK] total presets: {len(presets)}")

if __name__ == "__main__":
    main()
