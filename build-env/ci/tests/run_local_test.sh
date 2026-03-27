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

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# 获取项目根目录
PROJECT_ROOT="$( cd "$SCRIPT_DIR/../.." &> /dev/null && pwd )"

# 设置颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认参数
ARCH="arm"  # 改为新的架构命名
BUILD_DIR="$PROJECT_ROOT/build"
ROOTFS_DIR="$PROJECT_ROOT/rootfs"
TIMEOUT=300
VERBOSE=1
VERSION="latest"  # 添加版本参数

# 帮助信息
show_help() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -a, --arch     Target architecture (arm or aarch64)"
    echo "  -b, --build    Build directory (default: $BUILD_DIR)"
    echo "  -r, --rootfs   Rootfs directory (default: $ROOTFS_DIR)"
    echo "  -t, --timeout  Test timeout in seconds (default: 300)"
    echo "  -v, --version  Test case version (default: latest)"
    echo "  -q, --quiet    Disable verbose output"
    echo "  -h, --help     Show this help message"
}

log() {
    local level=$1
    shift
    if [ $VERBOSE -eq 1 ]; then
        case $level in
            "info")
                echo -e "${BLUE}[INFO]${NC} $*"
                ;;
            "warn")
                echo -e "${YELLOW}[WARN]${NC} $*"
                ;;
            "error")
                echo -e "${RED}[ERROR]${NC} $*"
                ;;
            "success")
                echo -e "${GREEN}[SUCCESS]${NC} $*"
                ;;
        esac
    fi
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--arch)
            ARCH="$2"
            shift 2
            ;;
        -b|--build)
            BUILD_DIR="$2"
            shift 2
            ;;
        -r|--rootfs)
            ROOTFS_DIR="$2"
            shift 2
            ;;
        -t|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -q|--quiet)
            VERBOSE=0
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# 验证架构
if [[ "$ARCH" != "arm" && "$ARCH" != "aarch64" ]]; then
    log "error" "Invalid architecture. Must be arm or aarch64"
    exit 1
fi

log "info" "Starting test setup for $ARCH"
log "info" "Build directory: $BUILD_DIR"
log "info" "Rootfs directory: $ROOTFS_DIR"
log "info" "Timeout: ${TIMEOUT}s"
log "info" "Test case version: $VERSION"

# 创建必要的目录
log "info" "Creating directories..."
mkdir -p "$ROOTFS_DIR/$ARCH"

# 检查rootfs文件是否存在，如果不存在则下载
if [ ! -f "$ROOTFS_DIR/$ARCH/rootfs.bin" ]; then
    log "info" "Downloading rootfs for $ARCH..."
    wget "http://192.168.11.87/repository/download/Intewell-N/rootfs/$ARCH/rootfs.bin" \
        -O "$ROOTFS_DIR/$ARCH/rootfs.bin"
    if [ $? -ne 0 ]; then
        log "error" "Failed to download rootfs"
        exit 1
    fi
    log "success" "Rootfs downloaded successfully"
else
    log "info" "Using existing rootfs file"
fi

# 检查内核文件是否存在
if [ ! -f "$BUILD_DIR/rtos.bin" ]; then
    log "error" "Kernel file not found at $BUILD_DIR/rtos.bin"
    log "info" "Please build the kernel first:"
    echo "mkdir -p build && cd build"
    echo "cmake .."
    echo "make ${ARCH}_defconfig"
    echo "make -j\$(nproc)"
    exit 1
fi

# 安装必要的工具
log "info" "Installing required tools..."
if ! command -v e2tools &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y e2tools
fi

# 运行测试
log "info" "Starting QEMU test for $ARCH..."
python3 "$SCRIPT_DIR/run_qemu_tests.py" \
    --kernel "$BUILD_DIR/rtos.bin" \
    --arch "$ARCH" \
    --timeout "$TIMEOUT" \
    --rootfs "$ROOTFS_DIR/$ARCH/rootfs.bin" \
    --version "$VERSION"

# 检查测试结果
TEST_RESULT=$?
if [ $TEST_RESULT -eq 0 ]; then
    log "success" "Tests passed successfully!"
else
    log "error" "Tests failed with exit code $TEST_RESULT"
    log "info" "Check test-results.xml and test-output.log for details"
fi

exit $TEST_RESULT
