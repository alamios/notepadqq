# Development

## Build

Use the dependencies listed in the [README](../README.md). CMake presets use
Ninja and separate output directories under `build/`:

```sh
cmake --preset dev
cmake --build --preset dev --parallel 4
```

`dev` produces a debug build; `release` produces an optimized build. Additional
presets include `ci`, `asan`, `ubsan` and `clang-tidy`. Build options are declared
in `CMakeLists.txt`. CodeMirror is the default editor; `NQQ_USE_MONACO` selects
the alternative backend, which does not support the fork's comment/comparison
commands.

## Isolated profiles

Close other Notepadqq instances, then run from the repository root:

```sh
mkdir -p build/profile/{config,data,cache}
env XDG_CONFIG_HOME="$PWD/build/profile/config" \
    XDG_DATA_HOME="$PWD/build/profile/data" \
    XDG_CACHE_HOME="$PWD/build/profile/cache" \
    ./build/dev/notepadqq
```

The application's single-instance identity is shared with other editions.
Separate XDG storage does not permit simultaneous upstream/fork instances.
Use disposable documents or copies when testing session recovery.

`./update-fork.sh` configures and builds `dev`, then installs into
`build/install/dev`. It preserves `build/profile` and does not create a menu
launcher. It resolves the checkout relative to its own path. Normal user
installation is separate; see [packaging](../PACKAGING.md).

## Tests

```sh
ctest --preset dev --output-on-failure
python3 scripts/test_install_user.py
```

CTest runs `ui-tests` against the real Qt/WebEngine editor and
`cpp-correctness-tests` for C++ behavior. The UI tests use temporary settings
and an offscreen Qt platform; CTest disables the WebEngine sandbox for that
process. Restricted environments must permit WebEngine's subprocesses and
sockets. Do not apply the test sandbox setting to normal application use.

For a focused UI run:

```sh
QT_QPA_PLATFORM=offscreen QTWEBENGINE_DISABLE_SANDBOX=1 \
    ./build/dev/ui-tests appearancePreferencesAlignment \
    tabAppearancePreferences forkTranslations
```

Tests cover comment dispatch and undo history; tab sizes, wrapping, movement
and metadata; session/backup round trips; comparison highlighting, editing
locks, cancellation and pane restoration; translations; and legacy telemetry
setting cleanup. Installer tests use temporary fixtures and mocked build
commands, never the real user installation.

Preview images are written under `build/dev` by selected UI tests. Native
Plasma/Wayland input and appearance should also be checked when a change
depends on desktop behavior. A passing offscreen suite alone does not establish
that all desktop styles or package formats have been validated.

## Documentation and ownership

When changing the application version, keep `CMakeLists.txt`, `POINTVERSION`
in `src/ui/include/notepadqq.h`, `snap/snapcraft.yaml`, AppStream release metadata,
the manual page and the README consistent. AppStream distinguishes development
versions from stable releases; editing metadata does not publish a GitHub release.
`POINTVERSION` supplies the About dialog and `--version` output. Historical
version values in compatibility tests are fixtures and should remain unchanged.

Keep the README and relevant guides current when behavior changes. Describe
the current implementation and its limits rather than development conversation
or obsolete package reports. Retain original copyright and third-party notices.
Vendored library documentation describes those libraries, not the maintained
fork, and should not be rebranded.

Work directly on `main` with focused commits. Use
[the contributor guide](../CONTRIBUTING.md) for bug-report details and contribution
conventions. GitHub workflows are in `.github/workflows`; their configured
platforms are not a claim that every fork build has been validated locally.
