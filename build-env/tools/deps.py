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
import os
import subprocess
import sys
from pathlib import Path

from gen_presets import load_yaml

FORCE_RESET = False  # 通过 --force 激活
OFFLINE_MODE = False
OFFLINE_MARKER = ".upboard-offline"

def is_dirty_git_repo(path):
    """返回 True 如果仓库脏"""
    if not os.path.exists(os.path.join(path, ".git")):
        return False
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=path, text=True)
    return bool(status.strip())

def is_git_repo(path):
    return os.path.exists(os.path.join(path, ".git"))


def has_ref(path, ref):
    return subprocess.call(
        ["git", "rev-parse", "--verify", ref],
        cwd=path,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ) == 0


def checkout_offline_ref(path, dep_name, ref):
    if not ref:
        return

    if subprocess.call(["git", "checkout", ref], cwd=path) == 0:
        print(f"[deps] offline checkout [{dep_name}] ref [{ref}]")
        return

    if has_ref(path, f"origin/{ref}"):
        print(f"[deps] offline checkout [{dep_name}] from origin/{ref}")
        subprocess.check_call(["git", "checkout", "-B", ref, f"origin/{ref}"], cwd=path)
        return

    raise RuntimeError(
        f"[deps] offline mode: ref [{ref}] not found in local repo [{dep_name}] at {path}"
    )

def checkout_local_ref(path, dep_name, ref):
    if not ref:
        return True

    if subprocess.call(["git", "checkout", ref], cwd=path) == 0:
        print(f"[deps] local checkout [{dep_name}] ref [{ref}]")
        return True

    if has_ref(path, f"origin/{ref}"):
        print(f"[deps] local checkout [{dep_name}] from origin/{ref}")
        subprocess.check_call(["git", "checkout", "-B", ref, f"origin/{ref}"], cwd=path)
        return True

    return False


def git_clone(dep):
    path = dep["path"]
    git_url = dep["git"]
    ref = dep.get("ref")

    if os.path.exists(path):
        if not is_git_repo(path):
            raise RuntimeError(f"[deps] path exists but is not a git repo: {path}")

        if is_dirty_git_repo(path):
            print(f"[deps] WARNING: repository [{dep['name']}] at {path} is dirty, skipping checkout/update")
            return

        if OFFLINE_MODE:
            print(f"[deps] offline mode: use local repository [{dep['name']}] at {path}")
            checkout_offline_ref(path, dep["name"], ref)

            if FORCE_RESET and ref:
                if has_ref(path, f"origin/{ref}"):
                    subprocess.check_call(["git", "reset", "--hard", f"origin/{ref}"], cwd=path)
                elif has_ref(path, ref):
                    subprocess.check_call(["git", "reset", "--hard", ref], cwd=path)
                else:
                    raise RuntimeError(
                        f"[deps] offline mode: cannot reset, ref [{ref}] not found for [{dep['name']}]"
                    )
                subprocess.check_call(["git", "clean", "-fd"], cwd=path)
            return

        # 仓库干净
        if FORCE_RESET:
            print(f"[deps] force updating [{dep['name']}] ref [{ref}] at {path}")
            subprocess.check_call(["git", "fetch", "--all"], cwd=path)

            # 1) 先切到目标 ref（分支/标签/commit 都行）
            if ref:
                subprocess.check_call(["git", "checkout", ref], cwd=path)
            else:
                subprocess.check_call(["git", "checkout", "HEAD"], cwd=path)

            # 2) 如果 ref 看起来是分支名：强制对齐到 origin/ref
            if ref:
                # origin/<ref> 存在才 reset；否则跳过（比如 ref 是 tag 或 commit hash）
                rc = subprocess.call(["git", "rev-parse", "--verify", f"origin/{ref}"], cwd=path,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if rc == 0:
                    subprocess.check_call(["git", "reset", "--hard", f"origin/{ref}"], cwd=path)
                    subprocess.check_call(["git", "clean", "-fd"], cwd=path)  # 可选：清理未跟踪文件
        else:
            print(f"[deps] sync local [{dep['name']}] at {path}")
            # 默认不走网络：共享依赖切板级时优先使用本地已有 refs。
            if ref and not checkout_local_ref(path, dep["name"], ref):
                print(f"[deps] ref [{ref}] not found locally, fetching [{dep['name']}]")
                subprocess.check_call(["git", "fetch", "--all"], cwd=path)
                subprocess.check_call(["git", "checkout", ref], cwd=path)
        return

    # 目录不存在 → clone + checkout
    if OFFLINE_MODE:
        raise RuntimeError(
            f"[deps] offline mode: missing dependency repo [{dep['name']}] at {path}, "
            f"please import offline package first"
        )

    print(f"[deps] cloning [{dep['name']}] -> {path}")
    subprocess.check_call(["git", "clone", git_url, path])
    if ref:
        subprocess.check_call(["git", "checkout", ref], cwd=path)

def ensure_dep_symlink(dep):
    """如果配置了 symlink_to，则把 dep.path 软链接到目标路径。"""
    link_path = dep.get("symlink_to")
    if not link_path:
        return

    src = os.path.abspath(dep["path"])
    dst = os.path.abspath(link_path)

    if not os.path.exists(src):
        raise FileNotFoundError(f"[deps] symlink source not found: {src}")

    os.makedirs(os.path.dirname(dst), exist_ok=True)

    if os.path.lexists(dst):
        if os.path.islink(dst):
            # Always recreate existing symlink to keep behavior deterministic.
            print(f"[deps] removing existing symlink: {dst}")
            os.unlink(dst)
        elif os.path.isdir(dst):
            if os.listdir(dst):
                raise RuntimeError(f"[deps] symlink target exists and is non-empty dir: {dst}")
            os.rmdir(dst)
        else:
            raise RuntimeError(f"[deps] symlink target exists and is not a link/dir: {dst}")

    rel_src = os.path.relpath(src, os.path.dirname(dst))
    os.symlink(rel_src, dst)
    print(f"[deps] symlink created: {dst} -> {rel_src}")

def _env_true(name):
    v = os.environ.get(name, "").strip().lower()
    return v in ("1", "true", "yes", "on")


def main(board_yaml_path):
    global FORCE_RESET
    global OFFLINE_MODE

    cfg = load_yaml(board_yaml_path)

    deps = cfg.get("dependencies", [])
    print(f"[deps] found {len(deps)} dependencies in {board_yaml_path}")

    for dep in deps:
        if "git" in dep:
            git_clone(dep)
        ensure_dep_symlink(dep)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Fetch/update board dependencies")
    parser.add_argument("board_config_file", help="board yaml file")
    parser.add_argument("--force", action="store_true", help="force reset to target ref")
    parser.add_argument("--offline", action="store_true", help="offline mode, do not fetch/clone from network")
    args = parser.parse_args()

    FORCE_RESET = args.force
    OFFLINE_MODE = args.offline or _env_true("UPBOARD_DEPS_OFFLINE") or Path(OFFLINE_MARKER).exists()
    if OFFLINE_MODE:
        print(f"[deps] offline mode enabled ({OFFLINE_MARKER})")

    try:
        main(args.board_config_file)
    except Exception as e:
        print(e)
        sys.exit(1)
