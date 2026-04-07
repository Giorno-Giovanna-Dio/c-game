#!/usr/bin/env bash
# 預設啟動 Python Host（亦可用 ./rogue_cli 純 C CLI）
set -euo pipefail
cd "$(dirname "$0")"
if [[ ! -f libroguecore.so ]]; then
  echo "請先執行: ./build.sh 或 make" >&2
  exit 1
fi
exec python3 host/app.py "$@"
