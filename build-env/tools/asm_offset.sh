#!/bin/bash
# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# 定义输入文件和输出文件
input_file=$1  # 输入文件路径
output_file=$2  # 输出宏定义文件路径

# 检查参数数量
if [ "$#" -ne 2 ]; then
    echo "使用方法: $0 输入文件.s 输出文件.h"
    exit 1
fi

# 清空或创建输出文件
> "$output_file"

# 使用 awk 提取 .ascii 行并生成宏定义
awk '/\.ascii/ && /->/ {
# 提取宏定义部分，汇编器生成的.S 中立即数的表示不同 [#$]? 用于提取不同的立即数前缀
match($0, /->([A-Za-z0-9_]+) [#$]?([0-9]+)/, arr);
if (arr[1] != "" && arr[2] != "") {
        macro_name = arr[1];        # 宏名称
        macro_value = arr[2];       # 宏值
        gsub("#", "", macro_value); # 去掉#符号

        # 输出宏定义到文件
        print "#define " macro_name " " macro_value >> "'"$output_file"'";
    }
}' "$input_file"

# 提示生成完成
#echo "宏定义已生成到 $output_file 中."

