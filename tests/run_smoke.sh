#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make all
printf 'q\n' | ./rogue_cli --seed 1 >/tmp/rc_smoke_cli.txt
python3 host/app.py --smoke
echo "smoke OK"
