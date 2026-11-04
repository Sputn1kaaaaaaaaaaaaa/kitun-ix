#!/usr/bin/env bash
set -euo pipefail

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "qemu-system-x86_64 is required but not installed." >&2
    exit 1
fi

make iso
qemu-system-x86_64 -m 2G -cdrom znu.iso -display none -monitor none -serial stdio
