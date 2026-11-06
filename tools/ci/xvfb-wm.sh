#!/usr/bin/env bash
# Runs a command on a virtual X display with a window manager. JUCE sets window properties
# that only exist once a window manager runs; without one, opening a dialog fails with BadAtom.
# Usage: tools/ci/xvfb-wm.sh <command> [args...]
set -euo pipefail
exec xvfb-run -a -s "-screen 0 1600x1200x24" bash -c 'openbox >/dev/null 2>&1 & sleep 2; exec "$@"' _ "$@"
