#!/usr/bin/env bash
# 於 Ubuntu 24.04：編譯 libroguecore.so 與 rogue_cli
set -euo pipefail
cd "$(dirname "$0")"
make clean
make all
echo "建置完成：libroguecore.so、rogue_cli"
