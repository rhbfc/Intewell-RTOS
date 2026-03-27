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

# ============================================================
# External Board / Kernel 管理脚本
#
# 支持的功能：
#
# 1. ext board / kernel 仓库管理
# 2. board / kernel 统一 dirty 检查策略
#    - 默认：仓库 dirty 时拒绝切换
#    - --force-board：允许继续
# 3. 安全切换策略
#    - board / kernel 默认不使用 git reset --hard
#    - index 仓库允许 reset --hard
# 4. index 支持：
#    - git index 仓库
#    - 本地 index 路径（--index）
# 5. --list：
#    - 列出 index 中所有可用的 board.yaml
#    - 输出不带 .yaml 后缀
# 6. offline 包：
#    - --offline-export 导出离线包
#    - --offline-import 导入离线包
# 7. 所有信息 / 警告 / 错误输出均带颜色
#
# ============================================================

import argparse
import io
import json
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
from datetime import datetime
from pathlib import Path

import yaml

# ------------------------------------------------------------
# 配置
# ------------------------------------------------------------
EXT_BASE_DIR = Path("boards-prj")
DEP_BASE_DIR = Path("dependencies")
INDEX_DIR = EXT_BASE_DIR / "index"
OFFLINE_MARKER = Path(".upboard-offline")
EXT_CONFIG_FILE = Path(__file__).resolve().parent.parent / "yaml" / "ext.yaml"

# ------------------------------------------------------------
# 颜色输出
# ------------------------------------------------------------
class C:
    RED     = "\033[31m"
    GREEN   = "\033[32m"
    YELLOW  = "\033[33m"
    BLUE    = "\033[34m"
    RESET   = "\033[0m"

def info(msg): print(f"{C.BLUE}[INFO]{C.RESET} {msg}")
def ok(msg):   print(f"{C.GREEN}[ OK ]{C.RESET} {msg}")
def warn(msg): print(f"{C.YELLOW}[WARN]{C.RESET} {msg}")
def err(msg):  print(f"{C.RED}[ERR ]{C.RESET} {msg}")

def die(msg, code=1):
    err(msg)
    sys.exit(code)


def get_index_repo_url() -> str:
    if EXT_CONFIG_FILE.exists():
        data = yaml.safe_load(EXT_CONFIG_FILE.read_text(encoding="utf-8")) or {}
        vars_cfg = data.get("vars", {})
        if isinstance(vars_cfg, dict) and "INDEX_REPO_URL" in vars_cfg:
            return vars_cfg["INDEX_REPO_URL"]
    return ""

# ------------------------------------------------------------
# Git 工具函数
# ------------------------------------------------------------
def run(cmd, cwd=None, check=True):
    r = subprocess.run(
        cmd, cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if check and r.returncode != 0:
        err("命令执行失败: " + " ".join(cmd))
        if r.stderr:
            print(r.stderr.strip())
        sys.exit(r.returncode)
    return r

def is_git_dirty(path: Path) -> bool:
    r = run(["git", "status", "--porcelain"], cwd=path, check=False)
    return bool(r.stdout.strip())


def list_git_repos(base_dir: Path) -> list[Path]:
    repos = []
    if not base_dir.exists():
        return repos

    for d in sorted(base_dir.iterdir()):
        if d.is_dir() and (d / ".git").exists():
            repos.append(d)
    return repos


def collect_dirty_repos(base_dirs: list[Path]) -> list[Path]:
    dirty = []
    for base in base_dirs:
        dirty.extend([p for p in list_git_repos(base) if is_git_dirty(p)])
    return dirty

# ------------------------------------------------------------
# index 仓库（允许 reset）
# ------------------------------------------------------------
def ensure_repo(path: Path, url: str):
    if not url or not url.strip():
        info("INDEX_REPO_URL 为空，跳过 index 仓库处理")
        return False

    if not path.exists():
        info(f"克隆 index 仓库: {url}")
        path.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", url, str(path)])
        ok("Index 克隆完成")
        return True

    # ok(f"Index 已存在: {path}")

    if not (path / ".git").exists():
        die(f"{path} 存在但不是 git 仓库")

    run(["git", "fetch", "origin"], cwd=path)
    r = run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=path)
    branch = r.stdout.strip()

    # info(f"重置 index 到 origin/{branch}")
    run(["git", "reset", "--hard", f"origin/{branch}"], cwd=path)
    # ok("Index 已与远端完全同步")
    return True

# ------------------------------------------------------------
# 统一 dirty 策略
# ------------------------------------------------------------
def dirty_guard(path: Path, name: str, force: bool):
    if not path.exists() or not (path / ".git").exists():
        return

    if not is_git_dirty(path):
        return

    msg = f"{name} 仓库存在未提交修改: {path}"

    if force:
        warn(msg + "（已强制继续）")
        return

    die(msg + f"（使用 --force-{name} 强制）")

# ------------------------------------------------------------
# Index 处理
# ------------------------------------------------------------
def load_index(index_arg: str | None) -> Path:
    if index_arg:
        p = Path(index_arg)
        if not p.exists():
            die(f"Index 路径不存在: {p}")
        ok(f"使用本地 index: {p}")
        return p

    if not ensure_repo(INDEX_DIR, get_index_repo_url()):
        return None

    return INDEX_DIR

def list_boards(index_dir: Path):
    yamls = sorted(index_dir.glob("*.yaml"))
    if not yamls:
        warn("Index 中未找到任何 yaml")
        return

    ok("Index 中可选的板级拓展包:")
    for y in yamls:
        print(f"  - {y.stem}")


# ------------------------------------------------------------
# Offline 包处理
# ------------------------------------------------------------
def _safe_extract_tar(tar: tarfile.TarFile, target_dir: Path):
    base = target_dir.resolve()
    for m in tar.getmembers():
        candidate = (base / m.name).resolve()
        if candidate != base and not str(candidate).startswith(str(base) + os.sep):
            die(f"离线包非法路径: {m.name}")
    tar.extractall(path=target_dir)


def export_offline_package(output_pkg: Path):
    export_dirs = [d for d in [EXT_BASE_DIR, DEP_BASE_DIR] if d.exists()]
    if not export_dirs:
        die(f"未找到可导出目录: {EXT_BASE_DIR} / {DEP_BASE_DIR}")

    if output_pkg.exists() and output_pkg.is_dir():
        die(f"输出路径是目录，需指定文件路径: {output_pkg}")

    output_pkg.parent.mkdir(parents=True, exist_ok=True)
    out = output_pkg.resolve()

    info(f"导出离线包: {out}")

    repos = {
        str(EXT_BASE_DIR): [p.name for p in list_git_repos(EXT_BASE_DIR)],
        str(DEP_BASE_DIR): [p.name for p in list_git_repos(DEP_BASE_DIR)],
    }
    manifest = {
        "format": "upboard-offline-v1",
        "created_at": datetime.now().isoformat(timespec="seconds"),
        "roots": [str(d) for d in export_dirs],
        "repos": repos,
    }
    manifest_data = json.dumps(manifest, ensure_ascii=False, indent=2).encode("utf-8")

    with tarfile.open(out, "w:gz") as tf:
        ti = tarfile.TarInfo(name="offline_bundle/manifest.json")
        ti.size = len(manifest_data)
        tf.addfile(ti, io.BytesIO(manifest_data))
        for d in export_dirs:
            tf.add(d, arcname=f"offline_bundle/{d}")

    ok(f"离线包导出完成: {out}")


def _copytree_merge(src: Path, dst: Path):
    def _on_rm_error(func, path, exc_info):
        try:
            os.chmod(path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
            func(path)
        except OSError:
            raise

    def _remove_tree_force(path: Path):
        shutil.rmtree(path, onerror=_on_rm_error)

    def _copy2_force(src_file, dst_file):
        dstp = Path(dst_file)
        if dstp.exists():
            try:
                dstp.chmod(dstp.stat().st_mode | stat.S_IWUSR)
            except OSError:
                pass
        return shutil.copy2(src_file, dst_file)

    for item in src.iterdir():
        target = dst / item.name
        if item.is_dir():
            # git 仓库目录采用整目录替换，避免只读 pack 文件覆盖失败
            if (item / ".git").exists() and target.exists():
                _remove_tree_force(target)
                shutil.copytree(item, target, copy_function=_copy2_force)
            else:
                shutil.copytree(item, target, dirs_exist_ok=True, copy_function=_copy2_force)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            _copy2_force(item, target)


def import_offline_package(input_pkg: Path, force_import: bool):
    pkg = input_pkg.resolve()
    if not pkg.is_file():
        die(f"离线包不存在: {pkg}")

    dirty_repos = collect_dirty_repos([EXT_BASE_DIR, DEP_BASE_DIR])
    if dirty_repos and not force_import:
        dirty_list = ", ".join(str(p) for p in dirty_repos)
        die(f"检测到未提交修改，拒绝导入: {dirty_list}（使用 --force-import 强制）")
    if dirty_repos and force_import:
        warn("检测到 dirty 仓库，--force-import 生效，继续导入")

    info(f"导入离线包: {pkg}")

    tmp_dir = Path(tempfile.mkdtemp(prefix="upboard_offline_"))
    try:
        try:
            with tarfile.open(pkg, "r:*") as tf:
                _safe_extract_tar(tf, tmp_dir)
        except tarfile.TarError as e:
            die(f"离线包解析失败: {e}")

        bundle_root = tmp_dir / "offline_bundle"
        copied_any = False
        for name, dst in [("boards-prj", EXT_BASE_DIR), ("dependencies", DEP_BASE_DIR)]:
            src = bundle_root / name
            if not src.is_dir():
                legacy = tmp_dir / name
                if legacy.is_dir():
                    src = legacy
                else:
                    continue
            dst.mkdir(parents=True, exist_ok=True)
            try:
                _copytree_merge(src, dst)
            except (OSError, shutil.Error) as e:
                die(f"离线包导入失败: {dst} <- {src}: {e}")
            copied_any = True

        if not copied_any:
            die("离线包格式无效，未找到 boards-prj/dependencies 目录")
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)

    OFFLINE_MARKER.write_text(
        f"# Auto generated by upboard offline import\n"
        f"imported_at={datetime.now().isoformat(timespec='seconds')}\n",
        encoding="utf-8",
    )

    ok(f"离线包导入完成: {pkg}")
    ok(f"已启用离线依赖模式: {OFFLINE_MARKER}")

# ------------------------------------------------------------
# Board 仓库管理
# ------------------------------------------------------------
def update_board_repo(
    prjname: str,
    repo_url: str,
    ref: str,
    base_dir: Path,
    force: bool = False,
):
    board_dir = base_dir / prjname

    info(f"Board 名称 : {prjname}")
    info(f"Board repo : {repo_url}")
    info(f"Board ref  : {ref}")
    info(f"Board dir  : {board_dir}")

    # ---- clone ----
    if not (board_dir / ".git").exists():
        if board_dir.exists() and any(board_dir.iterdir()):
            die(f"{board_dir} 已存在但不是 git 仓库")

        info("克隆 board 仓库")
        board_dir.parent.mkdir(parents=True, exist_ok=True)
        run([
            "git", "-c", "protocol.file.allow=always",
            "clone", "-b", ref, repo_url, str(board_dir)
        ])
        ok("Board 克隆完成")
        return board_dir

    # ---- update ----
    if is_git_dirty(board_dir):
        if force:
            warn("Board dirty，--force-board 生效，强制 reset")
            run(["git", "fetch", "origin"], cwd=board_dir)
            run(["git", "reset", "--hard", f"origin/{ref}"], cwd=board_dir)
        else:
            warn("Board dirty，跳过更新")
        return board_dir

    run(["git", "fetch", "origin"], cwd=board_dir)
    run(["git", "checkout", ref], cwd=board_dir)
    run(["git", "pull", "--ff-only"], cwd=board_dir)
    ok("Board 更新完成")
    return board_dir

# ------------------------------------------------------------
# Kernel 切换
# ------------------------------------------------------------
def safe_checkout(repo: Path, ref: str):
    info(f"切换 kernel 到 {ref}")
    run(["git", "fetch", "origin"], cwd=repo)
    run(["git", "checkout", ref], cwd=repo)
    ok(f"Kernel 已切换到 {ref}")

# ------------------------------------------------------------
# Kernel 处理
# ------------------------------------------------------------
def handle_kernel(board_dir: Path):
    kernel_yaml = board_dir / "kernel.yaml"
    if not kernel_yaml.exists():
        info("未找到 kernel.yaml, 跳过 kernel 切换")
        return

    data = yaml.safe_load(kernel_yaml.read_text()) or {}
    kernel = data.get("kernel", {})
    ref = kernel.get("ref")

    if not ref:
        warn("kernel.yaml 中未指定 kernel.ref, 跳过")
        return

    info(f"Kernel ref : {ref}")

    kernel_repo = Path.cwd()
    if not (kernel_repo / ".git").exists():
        warn("当前目录不是 git 仓库, 跳过 kernel 切换")
        return

    if is_git_dirty(kernel_repo):
        warn("Kernel 仓库存在未提交修改, 跳过切换")
        return

    safe_checkout(kernel_repo, ref)


# ------------------------------------------------------------
# 主流程
# ------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        prog="tbox --ext",
    )
    parser.add_argument("--list", action="store_true", help="板级拓展包list")
    parser.add_argument("--board", help="选择拉取的板级拓展包名称")
    parser.add_argument("--index", help="板级拓展包索引文件的本地路径")
    parser.add_argument("--force-board", action="store_true")
    parser.add_argument("--offline-export", help="导出当前 boards-prj + dependencies 为离线包（tar.gz）")
    parser.add_argument("--offline-import", help="导入离线包（tar/tar.gz）到 boards-prj + dependencies")
    parser.add_argument("--force-import", action="store_true", help="导入离线包时允许覆盖 dirty 仓库")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    args = parser.parse_args()

    op_count = sum([
        1 if args.list else 0,
        1 if args.board else 0,
        1 if args.offline_export else 0,
        1 if args.offline_import else 0,
    ])
    if op_count == 0:
        die("未指定操作，请使用 --list / --board / --offline-export / --offline-import")
    if op_count > 1:
        die("一次只能执行一个操作：--list / --board / --offline-export / --offline-import")

    if args.offline_export:
        export_offline_package(Path(args.offline_export))
        return

    if args.offline_import:
        import_offline_package(Path(args.offline_import), args.force_import)
        return

    index_dir = load_index(args.index)
    if index_dir is None:
        ok("未配置 index 地址，跳过后续行为")
        return

    if args.list:
        list_boards(index_dir)
        return

    if not args.board:
        die("未指定 --board")

    board_yaml = index_dir / f"{args.board}.yaml"
    if not board_yaml.exists():
        die(f"Index 中不存在 board 描述文件: {board_yaml}")

    # -------- 解析 board.yaml --------
    data = yaml.safe_load(board_yaml.read_text())

    boards = data.get("boards", {})
    repo = boards.get("repo")
    ref = boards.get("ref")

    if not repo or not ref:
        die("board.yaml 中缺少 boards.repo 或 boards.ref")

    board_dir = update_board_repo(
        args.board, repo, ref, EXT_BASE_DIR, args.force_board
    )

    # -------- Kernel 切换 --------
    handle_kernel(board_dir)

    ok("操作完成")

# ------------------------------------------------------------
if __name__ == "__main__":
    main()
