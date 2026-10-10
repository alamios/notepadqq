# Packaging Notes

## Fork development isolation

### Updating the local installation

Close the running fork, then execute `./update-fork.sh` from the repository root
(or invoke the script by its path from any other directory). It configures and
builds the `dev` preset and explicitly installs into the repository's
`build/install/dev` prefix. No administrator privileges are required. The
existing `Notepadqq (Fork)` launcher continues to use that prefix; settings and
sessions under `build/profile` are retained. The script installs the current
checkout, including uncommitted edits, without downloading source updates.

### Running with isolated data

The first fork feature is available in the local development binary after
building: **Edit > Toggle Comment**, default `Ctrl+Shift+C`, for CodeMirror.
It has not been packaged into a fork AppImage or installed into the user menu.

Run fork builds from their build output before installing them. Keep the
Flatpak, AppImage, and distribution package available until the fork passes
the acceptance checks in [the requirements](docs/FORK_REQUIREMENTS.md).

For example, launch a development build with repository-local storage:

```sh
mkdir -p build/profile/{config,data,cache}
env XDG_CONFIG_HOME="$PWD/build/profile/config" \
    XDG_DATA_HOME="$PWD/build/profile/data" \
    XDG_CACHE_HOME="$PWD/build/profile/cache" \
    ./build/dev/notepadqq
```

Run this command from the repository root. Close other Notepadqq instances
first: the current single-instance socket uses a fixed application identity,
so separate XDG storage alone does not guarantee separate running instances.
Profile-aware instance identity is proposed in the fork design.

Do not test crash recovery against real user sessions. Use copied fixtures
inside the development profile, including unsaved documents. Future fork
packages need a distinct executable/desktop identity and storage namespace
before they can safely run alongside upstream. Packaging changes are planned,
not implemented by this documentation update.

## Canonical install layout

- executable: `/usr/bin/notepadqq`
- application data: `/usr/share/notepadqq`
- desktop file: `/usr/share/applications/notepadqq.desktop`
- metainfo: `/usr/share/metainfo/com.notepadqq.Notepadqq.metainfo.xml`

Upstream CMake installs the real executable directly as `notepadqq` and does not install a generic launcher wrapper.

## Packager responsibilities

Distribution-specific wrappers should only be added by packagers when they are required for that package format or runtime environment.

Historically, some packages used wrappers to apply desktop-environment-specific Qt workarounds such as:

- `QT_QPA_PLATFORMTHEME=""`
- `XDG_CURRENT_DESKTOP="GNOME"`

These are not treated as upstream runtime requirements. If a downstream package still needs one of these workarounds, keep it in that package's launcher, desktop entry, or equivalent runtime configuration.

## Snap

The Snap package keeps its own launcher and Qt runtime configuration under `snap/local/`. That logic is package-specific and intentionally remains outside the upstream CMake install rules.
