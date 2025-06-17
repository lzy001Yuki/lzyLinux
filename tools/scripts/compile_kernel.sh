#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 获取项目根目录（假设脚本在 tools/scripts 目录下）
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# 确保构建目录存在
mkdir -p "${PROJECT_ROOT}/build/kernel"

# 容器的用户名参数（你可以选择 root 或主机上的 UID 和 GID）
CONTAINER_USER="$(id -u):$(id -g)" # 修改为 "root" 如有权限问题

# 使用Docker容器进行编译
docker run --rm \
    -v "${PROJECT_ROOT}:/workspace" \
    -u ${CONTAINER_USER} \
    strangelinux-builder \
    bash -c "cd /workspace/linux-5.4.290 && \
    make KCONFIG_CONFIG=../tools/config/kernel/.config O=../build/kernel -j$(nproc) -s"