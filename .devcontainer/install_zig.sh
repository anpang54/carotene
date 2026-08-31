#!/bin/bash
set -euo pipefail

ZIG_VERSION=$(curl -s https://api.github.com/repos/ziglang/zig/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
ZIG_DOWNLOAD_URL="https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz"

sudo mkdir -p /usr/local/zig
curl -fsSL "$ZIG_DOWNLOAD_URL" | sudo tar -xJ -C /usr/local/zig --strip-components=1
sudo ln -sf /usr/local/zig/zig /usr/local/bin/zig

zig version

# Based on https://github.com/NangiDev/zig-codespace-template
