#!/usr/bin/env bash
set -euo pipefail

exec pio run -e Loon_heltec_v4_r8_repeater "$@"
