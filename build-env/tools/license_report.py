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
import os
import re
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from xml.sax.saxutils import escape as xml_escape
from zipfile import ZIP_DEFLATED, ZipFile


DEFAULT_EXCLUDE_DIRS = {
    ".git",
    ".hg",
    ".svn",
    ".idea",
    ".vscode",
    "__pycache__",
    "build",
    "out",
}

DEFAULT_EXCLUDE_SUFFIXES = {
    ".auto",
    ".dtb",
    ".dts",
}

DEFAULT_BINARY_SUFFIXES = {
    ".a",
    ".bin",
    ".bmp",
    ".class",
    ".dll",
    ".dtb",
    ".dtbo",
    ".dylib",
    ".elf",
    ".exe",
    ".gif",
    ".gz",
    ".ico",
    ".jar",
    ".jpeg",
    ".jpg",
    ".ko",
    ".lib",
    ".mp3",
    ".mp4",
    ".o",
    ".obj",
    ".pdf",
    ".png",
    ".pyc",
    ".so",
    ".tar",
    ".xz",
    ".zip",
    ".7z",
}

SPDX_RE = re.compile(r"SPDX-License-Identifier:\s*([^\r\n*]+)")
COPYRIGHT_TAG_RE = re.compile(r"@copyright\b(.*)", re.IGNORECASE)
COPYRIGHT_LINE_RE = re.compile(r"\bcopyright\b", re.IGNORECASE)
ALL_RIGHTS_RESERVED_RE = re.compile(r"all rights reserved\.?", re.IGNORECASE)
LEADING_COPYRIGHT_MARK_RE = re.compile(r"^(?:copyright\b\s*)?(?:\(\s*c\s*\)|©)?\s*", re.IGNORECASE)


def compile_license_pattern(*patterns):
    return re.compile("|".join(f"(?:{pattern})" for pattern in patterns), re.IGNORECASE | re.DOTALL)


LICENSE_PATTERNS = [
    (
        "Apache-2.0",
        compile_license_pattern(r"apache license,\s*version\s*2(?:\.0)?", r"\bapache[- ]?2(?:\.0)?\b"),
    ),
    (
        "Apache-1.1",
        compile_license_pattern(r"apache license,\s*version\s*1\.1", r"\bapache[- ]?1\.1\b"),
    ),
    ("MIT-0", compile_license_pattern(r"\bmit[- ]?0\b", r"mit no attribution license")),
    ("MIT", compile_license_pattern(r"\bmit license\b", r"\bexpat license\b")),
    ("BSD-4-Clause", compile_license_pattern(r"\bbsd 4-clause\b", r"\boriginal bsd\b")),
    (
        "BSD-3-Clause",
        compile_license_pattern(
            r"\bbsd 3-clause\b",
            r"\bnew bsd\b",
            r"\bmodified bsd\b",
            r"redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:.{0,800}neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission",
        ),
    ),
    (
        "BSD-2-Clause",
        compile_license_pattern(r"\bbsd 2-clause\b", r"\bsimplified bsd\b", r"\bfreebsd license\b"),
    ),
    (
        "MulanPSL-2.0",
        compile_license_pattern(
            r"mulan permissive software license(?:,\s*version)?\s*2",
            r"木兰宽松许可证[，,]?\s*第?\s*2\s*版",
            r"\bmulan\s*psl\s*v?2\b",
            r"\bMulanPSL-?2(?:\.0)?\b",
        ),
    ),
    (
        "MulanPSL-1.0",
        compile_license_pattern(
            r"mulan permissive software license(?:,\s*version)?\s*1",
            r"木兰宽松许可证[，,]?\s*第?\s*1\s*版",
            r"\bmulan\s*psl\s*v?1\b",
            r"\bMulanPSL-?1(?:\.0)?\b",
        ),
    ),
    (
        "MPL-2.0",
        compile_license_pattern(r"mozilla public license(?:\s+version)?\s*2(?:\.0)?", r"\bMPL-?2(?:\.0)?\b"),
    ),
    (
        "MPL-1.1",
        compile_license_pattern(r"mozilla public license(?:\s+version)?\s*1\.1", r"\bMPL-?1\.1\b"),
    ),
    (
        "EPL-2.0",
        compile_license_pattern(r"eclipse public license(?:\s+version)?\s*2(?:\.0)?", r"\bEPL-?2(?:\.0)?\b"),
    ),
    (
        "EPL-1.0",
        compile_license_pattern(r"eclipse public license(?:\s+version)?\s*1(?:\.0)?", r"\bEPL-?1(?:\.0)?\b"),
    ),
    (
        "CDDL-1.0",
        compile_license_pattern(r"common development and distribution license(?:\s+version)?\s*1(?:\.0)?", r"\bCDDL-?1(?:\.0)?\b"),
    ),
    (
        "BSL-1.0",
        compile_license_pattern(r"boost software license(?:\s+version)?\s*1\.0", r"\bBSL-?1(?:\.0)?\b"),
    ),
    (
        "CC0-1.0",
        compile_license_pattern(r"creative commons zero\s*v?1\.0", r"\bCC0-?1(?:\.0)?\b"),
    ),
    ("Unlicense", compile_license_pattern(r"\bthe unlicense\b", r"\bunlicense\b")),
    ("Zlib", compile_license_pattern(r"\bzlib license\b", r"\bzlib/libpng license\b")),
    ("ISC", compile_license_pattern(r"\bisc license\b", r"\bpermission to use, copy, modify, and/or distribute this software\b")),
    (
        "LGPL-3.0-or-later",
        compile_license_pattern(
            r"(?:gnu lesser general public license|\blgpl\b).{0,160}\b(?:version\s*3|v3|3\.0)\b.{0,80}\bor later\b",
            r"\bLGPL[- ]?3(?:\.0)?\+\b",
            r"\bLGPL-3\.0-or-later\b",
        ),
    ),
    (
        "LGPL-2.1-or-later",
        compile_license_pattern(
            r"(?:gnu lesser general public license|\blgpl\b).{0,160}\b(?:version\s*2\.1|v2\.1|2\.1)\b.{0,80}\bor later\b",
            r"\bLGPL[- ]?2\.1\+\b",
            r"\bLGPL-2\.1-or-later\b",
        ),
    ),
    (
        "LGPL-2.0-or-later",
        compile_license_pattern(
            r"(?:gnu lesser general public license|\blgpl\b).{0,160}\b(?:version\s*2(?:\.0)?|v2|2\.0)\b.{0,80}\bor later\b",
            r"\bLGPL[- ]?2(?:\.0)?\+\b",
            r"\bLGPL-2\.0-or-later\b",
        ),
    ),
    (
        "LGPL-3.0-only",
        compile_license_pattern(
            r"gnu lesser general public license(?:\s+version)?\s*3(?:\.0)?",
            r"\bLGPL[- ]?3(?:\.0)?\b",
            r"\bLGPL-3\.0-only\b",
        ),
    ),
    (
        "LGPL-2.1-only",
        compile_license_pattern(
            r"gnu lesser general public license(?:\s+version)?\s*2\.1",
            r"\bLGPL[- ]?2\.1\b",
            r"\bLGPL-2\.1-only\b",
        ),
    ),
    (
        "LGPL-2.0-only",
        compile_license_pattern(
            r"gnu lesser general public license(?:\s+version)?\s*2(?:\.0)?",
            r"\bLGPL[- ]?2(?:\.0)?\b",
            r"\bLGPL-2\.0-only\b",
        ),
    ),
    ("LGPL", compile_license_pattern(r"lesser general public license", r"\blgpl\b")),
    (
        "AGPL-3.0-or-later",
        compile_license_pattern(
            r"(?:gnu affero general public license|\bagpl\b).{0,160}\b(?:version\s*3|v3|3\.0)\b.{0,80}\bor later\b",
            r"\bAGPL[- ]?3(?:\.0)?\+\b",
            r"\bAGPL-3\.0-or-later\b",
        ),
    ),
    (
        "AGPL-3.0-only",
        compile_license_pattern(
            r"gnu affero general public license(?:\s+version)?\s*3(?:\.0)?",
            r"\bAGPL[- ]?3(?:\.0)?\b",
            r"\bAGPL-3\.0-only\b",
        ),
    ),
    ("AGPL", compile_license_pattern(r"affero general public license", r"\bagpl\b")),
    (
        "GPL-3.0-or-later",
        compile_license_pattern(
            r"(?:gnu general public license|\bgpl\b).{0,160}\b(?:version\s*3|v3|3\.0)\b.{0,80}\bor later\b",
            r"\bGPL[- ]?3(?:\.0)?\+\b",
            r"\bGPL-3\.0-or-later\b",
        ),
    ),
    (
        "GPL-2.0-or-later",
        compile_license_pattern(
            r"(?:gnu general public license|\bgpl\b).{0,160}\b(?:version\s*2(?:\.0)?|v2|2\.0)\b.{0,80}\bor later\b",
            r"\bGPL[- ]?2(?:\.0)?\+\b",
            r"\bGPL-2\.0-or-later\b",
        ),
    ),
    (
        "GPL-1.0-or-later",
        compile_license_pattern(
            r"(?:gnu general public license|\bgpl\b).{0,160}\b(?:version\s*1(?:\.0)?|v1|1\.0)\b.{0,80}\bor later\b",
            r"\bGPL[- ]?1(?:\.0)?\+\b",
            r"\bGPL-1\.0-or-later\b",
        ),
    ),
    (
        "GPL-3.0-only",
        compile_license_pattern(
            r"gnu general public license(?:\s+version)?\s*3(?:\.0)?",
            r"\bGPL[- ]?3(?:\.0)?\b",
            r"\bGPL-3\.0-only\b",
        ),
    ),
    (
        "GPL-2.0-only",
        compile_license_pattern(
            r"gnu general public license(?:\s+version)?\s*2(?:\.0)?",
            r"\bGPL[- ]?2(?:\.0)?\b",
            r"\bGPL-2\.0-only\b",
        ),
    ),
    (
        "GPL-1.0-only",
        compile_license_pattern(
            r"gnu general public license(?:\s+version)?\s*1(?:\.0)?",
            r"\bGPL[- ]?1(?:\.0)?\b",
            r"\bGPL-1\.0-only\b",
        ),
    ),
    ("GPL", compile_license_pattern(r"gnu general public license", r"\bgpl\b", r"license:\s*gpl")),
]

GPL_FAMILY_TOKEN_RE = re.compile(r"(?<![A-Z0-9-])(?:AGPL|LGPL|GPL)(?:[-+0-9A-Z.a-z()]*)", re.IGNORECASE)
NEGATED_GPL_RE = re.compile(r"\b(?:no|without)\s+gpl(?:\s+\w+){0,4}\s+apply\b|\bno\s+gpl\s+restrictions\s+apply\b", re.IGNORECASE)
BSD_SOURCE_BINARY_RE = re.compile(
    r"redistribution and use in source and binary forms, with or without modification, are permitted",
    re.IGNORECASE | re.DOTALL,
)
BSD_3_CLAUSE_RE = re.compile(
    r"neither the name of the copyright holder nor the[\s*]+names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission",
    re.IGNORECASE | re.DOTALL,
)
BSD_2_CLAUSE_RE = re.compile(
    r"redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer",
    re.IGNORECASE | re.DOTALL,
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="递归扫描目录中的文本文件，并生成许可证识别的 Markdown/Excel 报告。"
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="要扫描的根目录，默认当前目录。",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="license-scan-report.md",
        help="Markdown 报告输出路径，默认 license-scan-report.md。",
    )
    parser.add_argument(
        "--excel-output",
        help="Excel 报告输出路径，支持 .xlsx。",
    )
    parser.add_argument(
        "--header-bytes",
        type=int,
        default=16384,
        help="每个文件读取用于许可证识别的头部字节数，默认 16384。",
    )
    parser.add_argument(
        "--include-hidden",
        action="store_true",
        help="包含隐藏文件和隐藏目录。",
    )
    parser.add_argument(
        "--exclude-dir",
        action="append",
        default=[],
        help="额外排除的目录名，可重复传入。",
    )
    return parser.parse_args()


def is_probably_binary(path):
    if path.suffix.lower() in DEFAULT_BINARY_SUFFIXES:
        return True

    try:
        with path.open("rb") as handle:
            chunk = handle.read(4096)
    except OSError:
        return False

    if b"\x00" in chunk:
        return True

    if not chunk:
        return False

    decoded = None
    try:
        decoded = chunk.decode("utf-8")
    except UnicodeDecodeError:
        # 采样块可能刚好截断在 UTF-8 多字节字符中间，忽略残缺尾字节即可。
        decoded = chunk.decode("utf-8", errors="ignore")

    if decoded is None:
        return True

    control_count = 0
    for ch in decoded:
        code = ord(ch)
        if ch in "\n\r\t\b\f":
            continue
        if code < 32 or 0x7F <= code <= 0x9F:
            control_count += 1

    return control_count / len(decoded) > 0.05


def read_header_text(path, header_bytes):
    try:
        with path.open("rb") as handle:
            raw = handle.read(header_bytes)
    except OSError as exc:
        return None, f"read_error: {exc}"

    if b"\x00" in raw:
        return None, "binary_null_byte"

    try:
        return raw.decode("utf-8"), None
    except UnicodeDecodeError:
        try:
            return raw.decode("utf-8", errors="ignore"), None
        except UnicodeDecodeError:
            try:
                return raw.decode("latin-1"), None
            except UnicodeDecodeError as exc:
                return None, f"decode_error: {exc}"


def normalize_spdx(expression):
    return " ".join(expression.strip().rstrip("*/#;").split())


def prune_generic_license_names(detected):
    versioned_families = {name.split("-", 1)[0] for name in detected if any(ch.isdigit() for ch in name)}
    pruned = []
    for name in detected:
        family = name.split("-", 1)[0]
        if name == family and family in versioned_families:
            continue
        if name not in pruned:
            pruned.append(name)
    return pruned


def infer_protocols(license_name, result, note):
    if not license_name:
        return ""

    if "版权声明" in note:
        return "版权声明"

    protocol_patterns = [
        ("Apache", re.compile(r"\bApache\b", re.IGNORECASE)),
        ("MIT", re.compile(r"\bMIT(?:-0)?\b", re.IGNORECASE)),
        ("BSD", re.compile(r"\bBSD(?:-\d-Clause)?\b", re.IGNORECASE)),
        ("MulanPSL", re.compile(r"\bMulanPSL\b|\bMulan PSL\b|木兰宽松许可证", re.IGNORECASE)),
        ("MPL", re.compile(r"\bMPL(?:-\d(?:\.\d)?)?\b", re.IGNORECASE)),
        ("EPL", re.compile(r"\bEPL(?:-\d(?:\.\d)?)?\b", re.IGNORECASE)),
        ("CDDL", re.compile(r"\bCDDL(?:-\d(?:\.\d)?)?\b", re.IGNORECASE)),
        ("BSL", re.compile(r"\bBSL(?:-\d(?:\.\d)?)?\b|\bBoost Software License\b", re.IGNORECASE)),
        ("CC0", re.compile(r"\bCC0(?:-\d(?:\.\d)?)?\b|Creative Commons Zero", re.IGNORECASE)),
        ("Unlicense", re.compile(r"\bUnlicense\b", re.IGNORECASE)),
        ("Zlib", re.compile(r"\bZlib\b", re.IGNORECASE)),
        ("ISC", re.compile(r"\bISC\b", re.IGNORECASE)),
        ("LGPL", re.compile(r"\bLGPL(?:-\d(?:\.\d)?(?:-[A-Za-z-]+)?)?\b", re.IGNORECASE)),
        ("AGPL", re.compile(r"\bAGPL(?:-\d(?:\.\d)?(?:-[A-Za-z-]+)?)?\b", re.IGNORECASE)),
        ("GPL", re.compile(r"\bGPL(?:-\d(?:\.\d)?(?:-[A-Za-z-]+)?)?\b", re.IGNORECASE)),
    ]

    protocols = []
    for protocol, pattern in protocol_patterns:
        if pattern.search(license_name) and protocol not in protocols:
            protocols.append(protocol)

    if result == "GPL" and not protocols:
        return "GPL"

    return " | ".join(protocols)


def normalize_comment_text(value):
    return " ".join(value.strip().rstrip("*/#;").split())


def normalize_owner_aliases(value):
    value = value.replace("科银", "科东")
    value = re.sub(r"kyland", "科东", value, flags=re.IGNORECASE)
    return value


def normalize_copyright_notice(value):
    value = normalize_comment_text(value)
    value = LEADING_COPYRIGHT_MARK_RE.sub("", value, count=1)
    value = normalize_owner_aliases(value)
    return normalize_comment_text(value)


def extract_comment_payload(line):
    text = line.strip()
    if not text:
        return ""

    for prefix in ("/**", "/*", "//", "*", "#", ";"):
        if text.startswith(prefix):
            text = text[len(prefix) :]
            break

    return normalize_comment_text(text)


def extract_copyright_notices(text):
    notices = []
    seen = set()

    def append_notice(notice):
        normalized = normalize_copyright_notice(notice)
        if not normalized:
            return
        lowered = normalized.lower()
        if lowered in seen:
            return
        seen.add(lowered)
        notices.append(normalized)

    for line in text.splitlines():
        payload = extract_comment_payload(line)
        if not payload:
            continue

        match = COPYRIGHT_TAG_RE.search(payload)
        if match:
            append_notice(match.group(1))
            continue

        if "版权所有" in payload:
            append_notice(payload)
            continue

        if COPYRIGHT_LINE_RE.search(payload):
            append_notice(payload)
            continue

        if ALL_RIGHTS_RESERVED_RE.fullmatch(payload) and notices:
            last_notice = notices[-1]
            if not ALL_RIGHTS_RESERVED_RE.search(last_notice) and re.search(
                r"\d{4}|[A-Za-z]|有限公司", last_notice
            ):
                combined = f"{last_notice} {payload}"
                notices[-1] = normalize_copyright_notice(combined)
                seen.discard(last_notice.lower())
                seen.add(notices[-1].lower())
    return notices


def classify_license(text):
    spdx_match = SPDX_RE.search(text)
    detected = []
    evidence = ""
    copyright_notices = extract_copyright_notices(text)

    if spdx_match:
        spdx_expr = normalize_spdx(spdx_match.group(1))
        detected.append(spdx_expr)
        evidence = f"SPDX: {spdx_expr}"
    else:
        for name, pattern in LICENSE_PATTERNS:
            if pattern.search(text):
                detected.append(name)
        detected = prune_generic_license_names(detected)
        if "GPL" in detected and NEGATED_GPL_RE.search(text) and not re.search(
            r"gnu general public license", text, re.IGNORECASE
        ):
            detected = [name for name in detected if name != "GPL"]
        if detected:
            evidence = "keywords: " + ", ".join(detected)

    if not detected:
        if BSD_SOURCE_BINARY_RE.search(text):
            if BSD_3_CLAUSE_RE.search(text):
                return "已识别", "BSD-3-Clause", "keywords: BSD-3-Clause", "检测到已识别许可证或版权声明"
            if BSD_2_CLAUSE_RE.search(text):
                return "已识别", "BSD-2-Clause", "keywords: BSD-2-Clause", "检测到已识别许可证或版权声明"
    if not detected and copyright_notices:
        notice = " | ".join(copyright_notices)
        return "已识别", notice, f"copyright: {notice}", "检测到版权声明"

    if not detected:
        return "未知", "", "未找到明确 SPDX 或许可证头", "需要人工确认"

    joined = " | ".join(detected)
    normalized = joined.upper()
    has_gpl = "GPL" in normalized
    has_non_gpl = any(
        token in normalized for token in ("APACHE", "BSD", "MIT", "ISC", "MPL", "MULAN", "INTEWELL")
    )
    has_or = " OR " in normalized or "/" in joined
    has_and = " AND " in normalized
    has_lgpl_only = "LGPL" in normalized and "GPL" not in normalized.replace("LGPL", "")

    if has_lgpl_only:
        return "已识别", joined, evidence, "检测到 LGPL，不按 GPL 归类"
    if has_gpl and has_non_gpl and has_or:
        return "双许可证（含GPL）", joined, evidence, "含 GPL 备选许可证"
    if has_gpl and has_non_gpl and has_and:
        return "GPL", joined, evidence, "组合许可证中包含 GPL"
    if has_gpl:
        gpl_tokens = sorted(set(token.upper() for token in GPL_FAMILY_TOKEN_RE.findall(joined)))
        note = "明确 GPL"
        if gpl_tokens:
            note = "明确 " + ", ".join(gpl_tokens)
        return "GPL", joined, evidence, note
    return "已识别", joined, evidence, "检测到已识别许可证或版权声明"


def escape_md_cell(value):
    value = value.replace("\n", " ").replace("\r", " ").strip()
    value = re.sub(r"\s+", " ", value)
    return value.replace("|", r"\|")


def iter_files(root, include_hidden, exclude_dirs):
    exclude_dirs = set(exclude_dirs)
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [
            name
            for name in dirnames
            if name not in exclude_dirs and (include_hidden or not name.startswith("."))
        ]
        for filename in filenames:
            if not include_hidden and filename.startswith("."):
                continue
            path = Path(dirpath) / filename
            if path.is_symlink():
                continue
            if path.suffix.lower() in DEFAULT_EXCLUDE_SUFFIXES:
                continue
            yield path


def analyze_file(path, root, header_bytes):
    rel_path = path.relative_to(root).as_posix()
    if is_probably_binary(path):
        return {
            "path": rel_path,
            "result": "跳过",
            "protocol": "",
            "license": "",
            "evidence": "",
            "note": "疑似二进制文件",
        }

    text, error = read_header_text(path, header_bytes)
    if text is None:
        return {
            "path": rel_path,
            "result": "跳过",
            "protocol": "",
            "license": "",
            "evidence": "",
            "note": error or "读取失败",
        }

    verdict, license_name, evidence, note = classify_license(text)
    return {
        "path": rel_path,
        "result": verdict,
        "protocol": infer_protocols(license_name, verdict, note),
        "license": license_name,
        "evidence": evidence,
        "note": note,
    }


def render_report(root, output, rows):
    counts = Counter(row["result"] for row in rows)
    lines = [
        "# License Scan Report",
        "",
        f"- Scan root: `{root}`",
        f"- Total files: `{len(rows)}`",
        f"- GPL-family: `{counts.get('GPL', 0)}`",
        f"- Dual license with GPL: `{counts.get('双许可证（含GPL）', 0)}`",
        f"- Recognized: `{counts.get('已识别', 0)}`",
        f"- Unknown: `{counts.get('未知', 0)}`",
        f"- Skipped: `{counts.get('跳过', 0)}`",
        "",
        "> 说明：该结果基于 SPDX 和文件头关键词做启发式识别，不构成法律意见；`未知` 项需要人工复核。",
        "",
        "| 文件 | 扫描结果 | 协议 | 识别到的许可证 | 证据 | 备注 |",
        "| --- | --- | --- | --- | --- | --- |",
    ]

    for row in rows:
        lines.append(
            "| {path} | {result} | {protocol} | {license} | {evidence} | {note} |".format(
                path=escape_md_cell(row["path"]),
                result=escape_md_cell(row["result"]),
                protocol=escape_md_cell(row["protocol"]),
                license=escape_md_cell(row["license"]),
                evidence=escape_md_cell(row["evidence"]),
                note=escape_md_cell(row["note"]),
            )
        )

    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_summary_rows(root, rows):
    counts = Counter(row["result"] for row in rows)
    return [
        ("Scan root", str(root)),
        ("Total files", str(len(rows))),
        ("GPL-family", str(counts.get("GPL", 0))),
        ("Dual license with GPL", str(counts.get("双许可证（含GPL）", 0))),
        ("Recognized", str(counts.get("已识别", 0))),
        ("Unknown", str(counts.get("未知", 0))),
        ("Skipped", str(counts.get("跳过", 0))),
        (
            "Note",
            "Heuristic result based on SPDX and file-header keywords only; unknown items require manual review.",
        ),
    ]


def sanitize_xml_text(value):
    text = str(value).replace("\r\n", "\n").replace("\r", "\n")
    return "".join(
        ch
        for ch in text
        if ch in "\n\t" or ord(ch) >= 0x20 and ch not in "\uFFFE\uFFFF"
    )


def excel_col_name(index):
    name = ""
    while index > 0:
        index, remainder = divmod(index - 1, 26)
        name = chr(65 + remainder) + name
    return name


def make_inline_cell(ref, value):
    safe_text = xml_escape(sanitize_xml_text(value))
    return f'<c r="{ref}" t="inlineStr"><is><t xml:space="preserve">{safe_text}</t></is></c>'


def make_excel_sheet_xml(root, rows, include_summary):
    summary_rows = build_summary_rows(root, rows) if include_summary else []
    headers = ["文件", "扫描结果", "协议", "识别到的许可证", "证据", "备注"]

    xml_rows = []
    row_num = 1

    for key, value in summary_rows:
        xml_rows.append(
            f'<row r="{row_num}">'
            f'{make_inline_cell(f"A{row_num}", key)}'
            f'{make_inline_cell(f"B{row_num}", value)}'
            f"</row>"
        )
        row_num += 1

    if summary_rows:
        row_num += 1

    header_row = row_num
    header_cells = "".join(
        make_inline_cell(f"{excel_col_name(index)}{header_row}", header)
        for index, header in enumerate(headers, start=1)
    )
    xml_rows.append(f'<row r="{header_row}">{header_cells}</row>')

    data_row_num = header_row + 1
    for row in rows:
        values = [row["path"], row["result"], row["protocol"], row["license"], row["evidence"], row["note"]]
        cells = "".join(
            make_inline_cell(f"{excel_col_name(index)}{data_row_num}", value)
            for index, value in enumerate(values, start=1)
        )
        xml_rows.append(f'<row r="{data_row_num}">{cells}</row>')
        data_row_num += 1

    last_row = max(header_row, data_row_num - 1)
    dimension = f"A1:F{max(last_row, 1)}"
    autofilter = f"A{header_row}:F{last_row}" if rows else f"A{header_row}:F{header_row}"

    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <dimension ref="{dimension}"/>
  <sheetViews>
    <sheetView workbookViewId="0">
      <pane ySplit="{header_row}" topLeftCell="A{header_row + 1}" activePane="bottomLeft" state="frozen"/>
    </sheetView>
  </sheetViews>
  <sheetFormatPr defaultRowHeight="15"/>
  <cols>
    <col min="1" max="1" width="60" customWidth="1"/>
    <col min="2" max="2" width="16" customWidth="1"/>
    <col min="3" max="3" width="20" customWidth="1"/>
    <col min="4" max="4" width="36" customWidth="1"/>
    <col min="5" max="5" width="48" customWidth="1"/>
    <col min="6" max="6" width="28" customWidth="1"/>
  </cols>
  <sheetData>
    {''.join(xml_rows)}
  </sheetData>
  <autoFilter ref="{autofilter}"/>
</worksheet>
"""


def top_level_bucket(path):
    return path.split("/", 1)[0] if "/" in path else "root"


def build_excel_sheets(root, rows):
    grouped_rows = {}
    for row in rows:
        grouped_rows.setdefault(top_level_bucket(row["path"]), []).append(row)

    sheets = [("Summary", make_excel_sheet_xml(root, rows, include_summary=True))]
    for name in sorted(grouped_rows):
        sheets.append((name[:31], make_excel_sheet_xml(root, grouped_rows[name], include_summary=False)))
    return sheets


def render_excel_report(root, output, rows):
    created_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    sheets = build_excel_sheets(root, rows)

    sheet_overrides = "\n".join(
        f'  <Override PartName="/xl/worksheets/sheet{index}.xml" '
        f'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
        for index, _ in enumerate(sheets, start=1)
    )

    content_types = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
{sheet_overrides}
  <Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
</Types>
"""
    root_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>
  <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>
</Relationships>
"""
    workbook_sheets = "\n".join(
        f'    <sheet name="{xml_escape(name)}" sheetId="{index}" r:id="rId{index}"/>'
        for index, (name, _) in enumerate(sheets, start=1)
    )
    workbook = f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheets>
{workbook_sheets}
  </sheets>
</workbook>
"""
    sheet_relationships = "\n".join(
        f'  <Relationship Id="rId{index}" '
        f'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" '
        f'Target="worksheets/sheet{index}.xml"/>'
        for index, _ in enumerate(sheets, start=1)
    )
    workbook_rels = f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
{sheet_relationships}
  <Relationship Id="rId{len(sheets) + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
</Relationships>
"""
    styles = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <fonts count="1">
    <font><sz val="11"/><name val="Calibri"/></font>
  </fonts>
  <fills count="2">
    <fill><patternFill patternType="none"/></fill>
    <fill><patternFill patternType="gray125"/></fill>
  </fills>
  <borders count="1">
    <border><left/><right/><top/><bottom/><diagonal/></border>
  </borders>
  <cellStyleXfs count="1">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0"/>
  </cellStyleXfs>
  <cellXfs count="1">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
  </cellXfs>
  <cellStyles count="1">
    <cellStyle name="Normal" xfId="0" builtinId="0"/>
  </cellStyles>
</styleSheet>
"""
    app = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties" xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">
  <Application>Codex</Application>
</Properties>
"""
    core = f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <dc:title>License Scan Report</dc:title>
  <dc:creator>Codex</dc:creator>
  <cp:lastModifiedBy>Codex</cp:lastModifiedBy>
  <dcterms:created xsi:type="dcterms:W3CDTF">{created_at}</dcterms:created>
  <dcterms:modified xsi:type="dcterms:W3CDTF">{created_at}</dcterms:modified>
</cp:coreProperties>
"""

    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "w", compression=ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", root_rels)
        archive.writestr("docProps/app.xml", app)
        archive.writestr("docProps/core.xml", core)
        archive.writestr("xl/workbook.xml", workbook)
        archive.writestr("xl/_rels/workbook.xml.rels", workbook_rels)
        archive.writestr("xl/styles.xml", styles)
        for index, (_, sheet_xml) in enumerate(sheets, start=1):
            archive.writestr(f"xl/worksheets/sheet{index}.xml", sheet_xml)


def main():
    args = parse_args()
    root = Path(args.root).resolve()
    output = Path(args.output).resolve()
    excel_output = Path(args.excel_output).resolve() if args.excel_output else None

    if not root.exists():
        print(f"scan root does not exist: {root}", file=sys.stderr)
        return 1

    exclude_dirs = DEFAULT_EXCLUDE_DIRS.union(args.exclude_dir)
    rows = []
    for path in iter_files(root, args.include_hidden, exclude_dirs):
        if path == output:
            continue
        if excel_output and path == excel_output:
            continue
        if path.is_file():
            rows.append(analyze_file(path, root, args.header_bytes))

    rows.sort(key=lambda item: item["path"])
    render_report(root, output, rows)
    print(f"report written to: {output}")
    if excel_output:
        render_excel_report(root, excel_output, rows)
        print(f"excel report written to: {excel_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
