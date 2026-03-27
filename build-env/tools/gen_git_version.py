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

import argparse
import os
import subprocess
import sys
from pathlib import Path


def run_git(repo: str, args: list[str]) -> str:
    """
    在 repo 路径下执行 git 命令；失败返回空串
    """
    try:
        out = subprocess.check_output(
            ["git", "-C", repo, *args],
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return out.strip()
    except Exception:
        return ""


def resolve_git_repo_root(path: str) -> str:
    """
    允许传入任意子目录（比如 dependencies/lwip/interface），
    自动解析到该目录所属的 git work tree 顶层路径。
    若不是 git repo，则返回原路径的绝对路径。
    """
    p = os.path.abspath(path)
    top = run_git(p, ["rev-parse", "--show-toplevel"])
    return top if top else p


def is_dirty(repo: str) -> bool:
    """
    判断工作区是否 dirty（忽略 submodules）
    """
    try:
        subprocess.check_call(
            ["git", "-C", repo, "diff", "--quiet", "--ignore-submodules"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.check_call(
            ["git", "-C", repo, "diff", "--cached", "--quiet", "--ignore-submodules"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return False
    except subprocess.CalledProcessError:
        return True
    except Exception:
        return False


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\"", "\\\"")


def get_repo_name(repo_root: str) -> str:
    """
    优先使用 remote.origin.url 推导 repo 名；没有 remote 则退回目录名
    """
    url = run_git(repo_root, ["config", "--get", "remote.origin.url"])
    if url:
        u = url.strip()

        # 取最后一段
        last = u.split("/")[-1]

        # 处理 git@host:group/name.git / group/name 这种
        if ":" in last and not last.endswith(".git"):
            last = last.split(":")[-1]

        # 去掉 .git
        if last.endswith(".git"):
            last = last[:-4]

        return last or "unknown"

    # fallback：repo 根目录名
    return os.path.basename(os.path.abspath(repo_root).rstrip(os.sep)) or "unknown"


def get_branch_name(repo_root: str) -> str:
    """
    分支名获取策略：
    1) 若非 detached：symbolic-ref -> 分支名
    2) detached：优先找包含该 commit 的 origin/* 远程分支
    3) 仍失败：用 name-rev 作为兜底
    4) 最后兜底：rev-parse --abbrev-ref HEAD
    """
    # 1) 正常分支
    b = run_git(repo_root, ["symbolic-ref", "--short", "-q", "HEAD"])
    if b and b != "HEAD":
        return b

    # 当前提交
    head = run_git(repo_root, ["rev-parse", "HEAD"])
    if head:
        # 2) detached：找包含该 commit 的远程分支（优先 origin）
        # 输出形如 refs/remotes/origin/xxx
        refs = run_git(repo_root, [
            "for-each-ref",
            "--format=%(refname)",
            "refs/remotes/origin"
        ])

        # 只有在 refs 存在时才走精确 contains（避免某些极简环境空输出）
        if refs:
            contains = run_git(repo_root, [
                "for-each-ref",
                "--contains", head,
                "--format=%(refname:short)",
                "refs/remotes/origin"
            ])
            if contains:
                # 可能多行，挑一个最像分支的（第一行即可）
                line = contains.splitlines()[0].strip()
                if line.startswith("origin/"):
                    return line[len("origin/"):] or "unknown"
                return line or "unknown"

    # 3) name-rev 兜底（可能是 origin/foo~3 之类）
    name = run_git(repo_root, ["name-rev", "--name-only", "--no-undefined", "HEAD"])
    if name:
        name = name.strip()
        name = name.split("~", 1)[0].split("^", 1)[0]
        if name.startswith("remotes/"):
            name = name[len("remotes/"):]
        if name.startswith("origin/"):
            name = name[len("origin/"):]
        return name or "unknown"

    # 4) 最后兜底
    b = run_git(repo_root, ["rev-parse", "--abbrev-ref", "HEAD"])
    return (b.strip() if b else "unknown") or "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True, help="Path to git repo root (or any subdir under it)")
    ap.add_argument("--out", required=True, help="Output header path")
    ns = ap.parse_args()

    # 允许传入子目录：自动 resolve 到真正 git root
    # print(f"Generating git version header for repo at: {ns.repo}  {ns.out}")
    repo_root = resolve_git_repo_root(ns.repo)
    out_path = Path(ns.out)

    repo_name = get_repo_name(repo_root)

    short_hash = run_git(repo_root, ["rev-parse", "--short", "HEAD"]) or "unknown"
    dirty = is_dirty(repo_root)
    git_hash = f"{short_hash}-dirty" if dirty and short_hash != "unknown" else short_hash

    branch = get_branch_name(repo_root)

    repo_name = c_escape(repo_name)
    git_hash = c_escape(git_hash)
    branch = c_escape(branch)

    content = (
        "/* auto-generated, do not edit */\n"
        f"#define GIT_REPO   \"{repo_name}\"\n"
        f"#define GIT_HASH   \"{git_hash}\"\n"
        f"#define GIT_BRANCH \"{branch}\"\n"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
