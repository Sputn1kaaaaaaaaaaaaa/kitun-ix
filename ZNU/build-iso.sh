#!/usr/bin/env bash
set -euo pipefail

exec make iso ISO="${1:-znu.iso}"
