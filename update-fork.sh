#!/usr/bin/env bash
# Build and install the current checkout into its local development prefix.
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$repo_dir"

cmake --preset dev
cmake --build --preset dev --parallel 4
cmake --install build/dev --prefix "$repo_dir/build/install/dev"

printf '\nFork updated. Open Notepadqq (Fork) from the application menu.\n'
