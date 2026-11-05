#!/usr/bin/env bash
# Host-only build of OwnShell EBOOT. Never flashes the console.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${PSPDEV:=$HOME/pspdev}"
export PSPDEV
export PATH="$PSPDEV/bin:$PATH"
if ! command -v psp-config >/dev/null; then
  echo "psp-config not found. Install pspdev v20261101 and set PSPDEV." >&2
  exit 1
fi
make -C "$ROOT/host_tests" test
make -C "$ROOT/shell"
ls -la "$ROOT/shell/EBOOT.PBP"
echo "Copy shell/EBOOT.PBP -> ms0:/PSP/GAME/OWNSHELL/EBOOT.PBP (manual)."
