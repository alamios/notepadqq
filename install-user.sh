#!/usr/bin/env bash
# Install this checkout as Notepadqq for the current user, without sudo.
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$repo_dir/scripts/install_user.py" "$@"
