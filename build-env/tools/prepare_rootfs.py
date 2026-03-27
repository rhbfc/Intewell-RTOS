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

# -*- coding: utf-8 -*-

"""
prepare_rootfs.py

设计目标（非常重要）：
  - 不做 include 的全量展开
  - 只从 board.yaml 中读取 rootfs.name
  - 只解析 rootfs.yaml
  - 只取被选中的那个 rootfs 定义

这是一个“rootfs 准备工具”，
不是通用 YAML 配置解释器。
"""

import os
import sys
import yaml
import hashlib
import urllib.request
import urllib.parse
import shutil
import re

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

VARS = {
    "YAML_ROOT": os.path.abspath(
        os.path.join(SCRIPT_DIR, "../yaml")
    ),
}

_VAR_PATTERN = re.compile(r"\$\{(\w+)\}")


# ============================================================
# 基础工具函数
# ============================================================
def expand_vars(s: str, vars_dict: dict) -> str:
    def repl(m):
        key = m.group(1)
        return vars_dict.get(key, m.group(0))  # 未定义则原样返回
    return _VAR_PATTERN.sub(repl, s)


def fatal(msg):
    """打印错误并退出"""
    print(f"[rootfs] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def info(msg):
    """普通信息输出"""
    print(f"[rootfs] {msg}")


def load_yaml(path):
    """加载 YAML 文件"""
    if not os.path.exists(path):
        fatal(f"yaml 文件不存在: {path}")
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def sha256sum(path):
    """计算文件 sha256"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def md5sum(path):
    """计算文件 md5"""
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


# ============================================================
# board / rootfs.yaml 解析
# ============================================================
def find_rootfs_yaml(board_path, board_cfg):
    """
    在 board.yaml 的 include 中查找 rootfs.yaml
    """
    base_dir = os.path.dirname(board_path)

    for inc in board_cfg.get("include", []):
        # ⭐ 新增：展开 ${YAML_ROOT}
        inc = expand_vars(inc, VARS)

        inc_path = inc
        if not os.path.isabs(inc):
            inc_path = os.path.normpath(os.path.join(base_dir, inc))

        if os.path.basename(inc_path) == "rootfs.yaml":
            return inc_path

    info("board include 中未找到 rootfs.yaml")


def load_selected_rootfs(board_path, board_cfg):
    """
    只支持两种方式：

    方式1（选择式）：
      board.yaml:
        rootfs:
          name: rootfs_aarch64
      并且 include 中能找到 rootfs.yaml（${YAML_ROOT}/rootfs.yaml 等）
      rootfs.yaml 支持：
        - rootfs: [ {name:...}, ... ]   # list
        - rootfs: {name:..., ...}       # dict（例如 rootfs: *rootfs_aarch64）

    方式2（内联式 / 方案2 anchor）：
      board.yaml:
        rootfs: *rootfs_aarch64
      这种情况下 include 可以不写 rootfs.yaml。
    """
    # ---------- 0) 基本校验 ----------
    rootfs_node = board_cfg.get("rootfs")
    if not isinstance(rootfs_node, dict):
        fatal("board.yaml 中 rootfs 必须是对象(dict)")

    sel = rootfs_node.get("name")
    if not sel or not isinstance(sel, str):
        fatal("board.yaml 未正确指定 rootfs.name（必须是字符串）")

    # ---------- 1) 方式2：board.yaml 内联了完整 rootfs ----------
    # anchor 展开后 rootfs_node 会包含这些关键字段之一
    if any(k in rootfs_node for k in ("source", "cache", "destination", "arch")):
        r = dict(rootfs_node)  # 不污染原 board_cfg
        # 相对路径以 board.yaml 所在目录为基准
        r["__base_dir__"] = os.path.dirname(os.path.abspath(board_path))
        info(f"选中 rootfs(内联): {sel}")
        return r

    # ---------- 2) 方式1：board.yaml 仅选择 name，需要去 rootfs.yaml 查 ----------
    rootfs_yaml = find_rootfs_yaml(board_path, board_cfg)
    if not rootfs_yaml:
        fatal("board.yaml 仅指定了 rootfs.name，但 include 中未找到 rootfs.yaml")

    info(f"使用 rootfs 定义文件: {rootfs_yaml}")
    data = load_yaml(rootfs_yaml)

    entries = data.get("rootfs")
    if entries is None:
        fatal("rootfs.yaml 中缺少 rootfs")

    base_dir = os.path.dirname(rootfs_yaml)

    # 2.1 rootfs.yaml: rootfs 是 dict（例如 rootfs: *rootfs_aarch64）
    if isinstance(entries, dict):
        name = entries.get("name")
        if not name or not isinstance(name, str):
            fatal("rootfs.yaml 中 rootfs 是对象但缺少 name（或 name 非字符串）")
        if name != sel:
            fatal(f"board.yaml 选择 rootfs.name='{sel}'，但 rootfs.yaml 直接定义的是 '{name}'")

        r = dict(entries)
        r["__base_dir__"] = base_dir
        info(f"选中 rootfs: {sel}")
        return r

    # 2.2 rootfs.yaml: rootfs 是 list（老式列表定义）
    if not isinstance(entries, list):
        fatal("rootfs.yaml 中 rootfs 既不是列表也不是对象(dict)")

    for r in entries:
        if isinstance(r, dict) and r.get("name") == sel:
            rr = dict(r)
            rr["__base_dir__"] = base_dir
            info(f"选中 rootfs: {sel}")
            return rr

    fatal(f"rootfs.yaml 中未找到 rootfs: {sel}")


# ============================================================
# cache / 下载 / 校验
# ============================================================

def get_cache_dir():
    """
    获取 rootfs cache 目录
    """
    base = os.environ.get("XDG_CACHE_HOME")
    if not base:
        base = os.path.expanduser("~/.cache")
    path = os.path.join(base, "rootfs")
    os.makedirs(path, exist_ok=True)
    return path


def is_url(path):
    """判断是否 http/https URL"""
    p = urllib.parse.urlparse(path)
    return p.scheme in ("http", "https")


def resolve_local_path(path, base_dir):
    """
    解析本地路径：
      - file://xxx
      - 绝对路径
      - 相对路径（相对于 rootfs.yaml）
    """
    if path.startswith("file://"):
        return urllib.parse.urlparse(path).path
    if os.path.isabs(path):
        return path
    return os.path.normpath(os.path.join(base_dir, path))


def download(url, dst):
    """下载 URL 到指定文件"""
    info(f"下载 rootfs: {url}")
    tmp = dst + ".tmp"
    with urllib.request.urlopen(url) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f)
    os.rename(tmp, dst)


def verify_hash(path, cfg):
    """
    校验 hash（校验的是 rootfs 的来源文件）
    """
    if "sha256" in cfg:
        h = sha256sum(path)
        if h != cfg["sha256"]:
            fatal(f"sha256 校验失败: {path}")
        info("sha256 校验通过")

    if "md5" in cfg:
        h = md5sum(path)
        if h != cfg["md5"]:
            fatal(f"md5 校验失败: {path}")
        info("md5 校验通过")


def fetch_rootfs(rootfs):
    """
    获取 rootfs 源文件（URL 或 本地路径，自动判断）
    """
    source = rootfs.get("source", {})
    path = source.get("path")
    if not path:
        fatal("rootfs.source.path 未指定")

    base_dir = rootfs.get("__base_dir__", os.getcwd())

    cache_cfg = rootfs.get("cache", {})
    cache_enable = cache_cfg.get("enable", False)
    cache_key = cache_cfg.get("key", rootfs.get("name", "rootfs"))

    cache_dir = get_cache_dir()
    cache_file = os.path.join(cache_dir, cache_key + ".bin")

    # ---- URL ----
    if is_url(path):
        if cache_enable and os.path.exists(cache_file):
            info(f"命中 cache: {cache_file}")
        else:
            download(path, cache_file)
        verify_hash(cache_file, rootfs)
        return cache_file

    # ---- 本地文件 ----
    local = resolve_local_path(path, base_dir)
    if not os.path.exists(local):
        fatal(f"本地 rootfs 不存在: {local}")

    info(f"使用本地 rootfs: {local}")
    verify_hash(local, rootfs)
    return local


# ============================================================
# rootfs 安装（copy / link / hardlink）
# ============================================================

def install_rootfs(src, rootfs, out_path):
    """
    将 rootfs 安装为最终 rootfs.bin
    """
    mode = rootfs.get("destination", {}).get("mode", "copy")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    # 如果已存在，先删除（包括符号链接）
    if os.path.lexists(out_path):
        os.remove(out_path)

    if mode == "copy":
        shutil.copyfile(src, out_path)
        info(f"copy rootfs -> {out_path}")

    elif mode == "link":
        os.symlink(os.path.abspath(src), out_path)
        info(f"symlink rootfs -> {out_path}")

    elif mode == "hardlink":
        os.link(src, out_path)
        info(f"hardlink rootfs -> {out_path}")

    else:
        fatal(f"不支持的 destination.mode: {mode}")


# ============================================================
# 主流程
# ============================================================

def main():
    if len(sys.argv) != 3 or sys.argv[1] != "--out":
        print("用法: prepare_rootfs.py --out <rootfs.bin>")
        sys.exit(1)

    out_path = sys.argv[2]

    board_cfg_path = os.environ.get("BOARD_YAML")
    if not board_cfg_path:
        fatal("未设置 BOARD_YAML")

    info(f"使用 board 配置: {board_cfg_path}")

    # 1. 只加载 board.yaml（不展开 include）
    board_cfg = load_yaml(board_cfg_path)

    # 2. 只解析被选中的 rootfs
    rootfs = load_selected_rootfs(board_cfg_path, board_cfg)

    # 3. 获取并校验 rootfs 源文件
    src = fetch_rootfs(rootfs)

    # 4. 安装 rootfs
    install_rootfs(src, rootfs, out_path)

    info("rootfs 准备完成")


if __name__ == "__main__":
    main()
