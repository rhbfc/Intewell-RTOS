#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

DEFAULT_YEAR = "2026"
DEFAULT_HOLDER = "Kyland Inc."
DEFAULT_SOFTWARE = "Intewell-RTOS"


MULAN_LINES = [
    "Copyright (c) {year} {holder}",
    "{software} is licensed under Mulan PSL v2.",
    "You can use this software according to the terms and conditions of the Mulan PSL v2.",
    "You may obtain a copy of Mulan PSL v2 at:",
    "         http://license.coscl.org.cn/MulanPSL2",
    'THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,',
    "EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,",
    "MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.",
    "See the Mulan PSL v2 for more details.",
]

HEADER_MARKERS = (
    "Mulan PSL v2",
    "MulanPSL-2.0",
    "license.coscl.org.cn/MulanPSL2",
    "Copyright (c)",
    "This file is licensed under the Mulan Permissive Software License v2.",
    "You can use this software according to the terms and conditions of the Mulan PSL v2.",
    'THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS',
    "EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,",
    "MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.",
    "See the Mulan PSL v2 for more details.",
)

LEGACY_HEADER_UUID = "B1C172A9-F6EB-A737-" "CD3A-CBCFCBDBE26B"
LEGACY_HEADER_MARKERS = {
    f"/* {LEGACY_HEADER_UUID} */",
}

MULAN_BLOCK_HEADER_RE = re.compile(
    r"^\s*/\*.*?Mulan PSL v2.*?See the Mulan PSL v2 for more details\.\s*\*/\s*",
    re.DOTALL,
)
LEADING_BLOCK_COMMENT_RE = re.compile(r"^\s*/\*.*?\*/\s*", re.DOTALL)
LEADING_HASH_COMMENT_RE = re.compile(r"^(?:[ \t]*#(?: |$).*(?:\r?\n|$))+[ \t]*(?:\r?\n)*")
HEADER_HINT_RE = re.compile(
    r"(?i)("
    r"mulan psl v2|mulanpsl-2\.0|license\.coscl\.org\.cn/mulanpsl2|"
    r"copyright|all rights reserved|kedong|intewell|kyland|"
    + re.escape(LEGACY_HEADER_UUID)
    + r"|科东|科银|版权所有)"
)


def parse_args():
    parser = argparse.ArgumentParser(description="为指定文件添加或刷新木兰 PSL v2 标准头。")
    parser.add_argument("files", nargs="+", help="要处理的文件列表。")
    return parser.parse_args()


def comment_style(path):
    name = path.name
    suffix = path.suffix.lower()

    if name == "asm-offsets":
        return "block"
    if name == "desc_template" or name.endswith(".json.template"):
        return "block"
    if path.parent.name == "vscode" and suffix == ".json":
        return "block"
    if name == "CMakeLists.txt" or name.startswith("Kconfig"):
        return "hash"
    if suffix in {".cmake", ".py", ".sh", ".mk", ".yaml", ".yml"}:
        return "hash"
    if suffix in {".c", ".h", ".cc", ".cpp", ".hpp", ".hh", ".S", ".s", ".lds", ".ld"}:
        return "block"
    return "hash"


def render_header(style, year, holder, software):
    lines = [line.format(year=year, holder=holder, software=software) for line in MULAN_LINES]
    if style == "block":
        body = "\n".join(f" * {line}" for line in lines)
        return f"/*\n{body}\n */\n\n"
    body = "\n".join(f"# {line}" for line in lines)
    return f"{body}\n\n"


def strip_existing_mulan_header(content):
    lines = content.splitlines(keepends=True)
    start = 0

    if lines and lines[0].startswith("#!"):
        start = 1

    end = start
    saw_marker = False

    while end < len(lines):
        stripped = lines[end].strip()

        if stripped == "":
            end += 1
            continue
        if stripped in LEGACY_HEADER_MARKERS:
            saw_marker = True
            end += 1
            continue
        if any(token in stripped for token in HEADER_MARKERS):
            saw_marker = True
            end += 1
            continue
        if saw_marker and stripped in {"#", "/*", "*", "*/"}:
            end += 1
            continue
        break

    if not saw_marker:
        return content

    remaining = "".join(lines[:start] + lines[end:])
    return remaining.lstrip("\n")


def strip_leading_comment_headers(content):
    shebang = ""
    body = content.lstrip("\ufeff")

    if body.startswith("#!"):
        shebang, _, body = body.partition("\n")
        shebang += "\n"

    while True:
        updated = body.lstrip("\ufeff\r\n")

        match = LEADING_BLOCK_COMMENT_RE.match(updated)
        if match and HEADER_HINT_RE.search(match.group(0)):
            body = updated[match.end() :]
            continue

        match = LEADING_HASH_COMMENT_RE.match(updated)
        if match and HEADER_HINT_RE.search(match.group(0)):
            body = updated[match.end() :]
            continue

        body = updated
        break

    return shebang + body


def strip_legacy_markers(content):
    lines = content.splitlines(keepends=True)
    cleaned = []

    for index, line in enumerate(lines):
        if index < 40 and line.strip() in LEGACY_HEADER_MARKERS:
            continue
        cleaned.append(line)

    return "".join(cleaned)


def detect_newline(raw):
    return "\r\n" if b"\r\n" in raw else "\n"


def strip_leading_headers(content, header):
    while content.startswith(header):
        content = content[len(header) :].lstrip("\r\n")
    return content


def strip_malformed_mulan_blocks(content):
    while True:
        updated = MULAN_BLOCK_HEADER_RE.sub("", content, count=1)
        if updated == content:
            return content
        content = updated.lstrip("\r\n")


def refresh_file(path):
    raw = path.read_bytes()
    newline = detect_newline(raw)
    original = raw.decode("utf-8").replace("\r\n", "\n")
    style = comment_style(path)
    header = render_header(style, DEFAULT_YEAR, DEFAULT_HOLDER, DEFAULT_SOFTWARE)
    match_header = header if newline == "\n" else header.replace("\n", newline)
    content = strip_leading_comment_headers(original)
    content = strip_existing_mulan_header(content)
    content = strip_legacy_markers(content)
    content = strip_leading_headers(content, match_header)
    content = strip_malformed_mulan_blocks(content)

    if content.startswith("#!"):
        shebang, _, rest = content.partition("\n")
        updated = f"{shebang}\n{header}{rest.lstrip(chr(10))}"
    else:
        updated = header + content.lstrip("\n")

    if newline != "\n":
        updated = updated.replace("\n", newline)

    path.write_text(updated, encoding="utf-8", newline="")


def main():
    args = parse_args()
    for file_name in args.files:
        path = Path(file_name).resolve()
        if not path.exists():
            raise FileNotFoundError(f"file not found: {path}")
        refresh_file(path)
        print(f"refreshed: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
