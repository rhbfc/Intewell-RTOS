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

import json
import os
from pathlib import Path

PRESETS = Path("CMakePresets.json")

def write_file(rel_dir: str, cond_var: str, cond_value: str, include_path: str):
    """
    在 boards/{rel_dir}/Kconfig.auto 生成：
      if {cond_var} = "{cond_value}"
          orsource "{include_path}"
      endif
    """
    kconfig_auto = Path(rel_dir) / "Kconfig.auto"
    kconfig_auto.parent.mkdir(parents=True, exist_ok=True)

    kconfig_auto.write_text(
        f'''# Auto generated, DO NOT EDIT
if {cond_var} = "{cond_value}"
    orsource "{include_path}"
endif
''',
        encoding="utf-8",
    )
    print(f"[create] {kconfig_auto}")


with PRESETS.open("r", encoding="utf-8") as f:
    data = json.load(f)

for preset in data.get("configurePresets", []):
    cache_vars = preset.get("cacheVariables", {})

    board_path = cache_vars.get("CONFIG_BOARD_PATH")
    if not board_path:
        continue

    # 1️⃣ board 级：BOARD_PATH
    write_file(
        rel_dir=board_path,
        cond_var="BOARD_PATH",
        cond_value=board_path,
        include_path="Kconfig",
    )
