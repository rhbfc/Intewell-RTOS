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
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, Optional


def slugify(raw: str) -> str:
    value = raw.strip().lower()
    value = re.sub(r"[^a-z0-9]+", "-", value).strip("-")
    return value or "board"


def quote_yaml_string(value: str) -> str:
    # JSON string is valid YAML string and escapes content safely.
    return json.dumps(value, ensure_ascii=False)


def infer_test_arch_from_preset(preset: str) -> Optional[str]:
    normalized = preset.strip().lower()
    if "qemu" not in normalized:
        return None

    if "x86_64" in normalized:
        return "x86_64"
    elif "aarch64" in normalized:
        return "aarch64"
    elif "aarch32" in normalized or "arm" in normalized:
        return "arm"
    else:
        return None


def parse_board_test_config(board: Dict[str, Any], preset: str, board_index: int) -> Optional[Dict[str, str]]:
    test_raw = board.get("test", False)

    if test_raw is None or test_raw is False:
        return None

    if isinstance(test_raw, bool):
        if not test_raw:
            return None
        test_obj: Dict[str, Any] = {}
    elif isinstance(test_raw, dict):
        enabled_raw = test_raw.get("enabled", True)
        if not isinstance(enabled_raw, bool):
            raise ValueError(
                f"Invalid board at index {board_index}: 'test.enabled' must be a boolean."
            )
        if not enabled_raw:
            return None
        test_obj = test_raw
    else:
        raise ValueError(
            f"Invalid board at index {board_index}: 'test' must be a boolean or object."
        )

    arch = str(test_obj.get("arch", "")).strip()
    if not arch:
        inferred_arch = infer_test_arch_from_preset(preset)
        if inferred_arch:
            arch = inferred_arch
    if not arch:
        raise ValueError(
            f"Invalid board at index {board_index}: test enabled but no arch provided and cannot infer from preset '{preset}'."
        )

    result: Dict[str, str] = {"arch": arch}

    timeout_raw = test_obj.get("timeout")
    if timeout_raw is not None:
        timeout_str = str(timeout_raw).strip()
        if not timeout_str.isdigit() or int(timeout_str) <= 0:
            raise ValueError(
                f"Invalid board at index {board_index}: 'test.timeout' must be a positive integer."
            )
        result["timeout"] = timeout_str

    version_raw = test_obj.get("version")
    if version_raw is not None:
        version = str(version_raw).strip()
        if not version:
            raise ValueError(
                f"Invalid board at index {board_index}: 'test.version' must be a non-empty string."
            )
        result["version"] = version

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate GitLab jobs from boards JSON.")
    parser.add_argument("--config", required=True, help="Path to boards json file.")
    parser.add_argument("--output", required=True, help="Path to generated child pipeline yaml.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config_path = Path(args.config)
    output_path = Path(args.output)

    if not config_path.is_file():
        print(f"Config file not found: {config_path}", file=sys.stderr)
        return 1

    data = json.loads(config_path.read_text(encoding="utf-8"))
    run_board_tests = os.getenv("RUN_BOARD_TESTS", "").strip().lower() in {
        "1",
        "true",
        "yes",
        "on",
    }
    boards = data.get("boards")
    if not isinstance(boards, list) or not boards:
        print("Invalid config: 'boards' must be a non-empty array.", file=sys.stderr)
        return 1

    lines = [
        "include:",
        "  - local: build-env/ci/boards-child-template.yml",
        "",
        "stages:",
        "  - build",
        "  - test",
        "  - package",
        "",
    ]

    used_job_names = set()
    build_job_names = []
    for index, board in enumerate(boards):
        if not isinstance(board, dict):
            print(f"Invalid board item at index {index}: expected object.", file=sys.stderr)
            return 1

        preset = str(board.get("preset", "")).strip()
        desc = str(board.get("desc", "")).strip() or preset or f"board-{index}"
        try:
            test_cfg = parse_board_test_config(board, preset, index)
        except ValueError as exc:
            print(str(exc), file=sys.stderr)
            return 1
        if not run_board_tests:
            test_cfg = None

        base_name = preset or desc or f"board-{index}"
        job_name = f"build_{index}_{slugify(base_name)}"
        while job_name in used_job_names:
            job_name = f"{job_name}-x"
        used_job_names.add(job_name)

        lines.extend(
            [
                f"{job_name}:",
                "  extends: .board_build_template",
                "  variables:",
                f'    BOARD_INDEX: "{index}"',
                f"    BOARD_DESC: {quote_yaml_string(desc)}",
                f"    BOARD_PRESET: {quote_yaml_string(preset)}",
                f'    RUN_QEMU_TEST: {"true" if test_cfg else "false"}',
                "",
            ]
        )
        build_job_names.append(job_name)

        if test_cfg:
            test_job_name = f"test_{index}_{slugify(base_name)}"
            while test_job_name in used_job_names:
                test_job_name = f"{test_job_name}-x"
            used_job_names.add(test_job_name)

            lines.extend(
                [
                    f"{test_job_name}:",
                    "  extends: .board_test_template",
                    "  needs:",
                    f"    - job: {job_name}",
                    "      artifacts: true",
                    "  variables:",
                    f"    BOARD_PRESET: {quote_yaml_string(preset)}",
                    f"    TEST_ARCH: {quote_yaml_string(test_cfg['arch'])}",
                    *(
                        [f'    TEST_TIMEOUT: "{test_cfg["timeout"]}"']
                        if "timeout" in test_cfg
                        else []
                    ),
                    *(
                        [f"    TEST_VERSION: {quote_yaml_string(test_cfg['version'])}"]
                        if "version" in test_cfg
                        else []
                    ),
                    "",
                ]
            )

    lines.extend(
        [
            "publish_board_artifacts:",
            "  extends: .board_publish_template",
            "  needs:",
            *[
                f"    - job: {job_name}\n      artifacts: true"
                for job_name in build_job_names
            ],
            "",
        ]
    )

    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Generated {len(boards)} jobs to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
