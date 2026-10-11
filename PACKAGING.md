# Packaging Notes

## Per-user installation as Notepadqq

Run `./install-user.sh` after closing all editions of Notepadqq. The script
resolves the checkout relative to itself, builds `release` with four parallel
jobs, stages the CMake installation in a temporary directory, then installs:

- executable: `~/.local/bin/notepadqq`
- editor assets: `~/.local/share/notepadqq`
- menu launcher: `~/.local/share/applications/notepadqq.desktop`
- icons, metainfo and manual: under `~/.local/share`
- settings and session caches: `~/.config/Notepadqq`

The name is **Notepadqq**, the desktop identity remains `notepadqq.desktop`,
and `Terminal=false`. All launcher actions use the absolute installed binary,
so the menu does not depend on the shell's `PATH`. It can be pinned in Plasma.
The installed application does not depend on the checkout's location; moving
or deleting the checkout does not remove installed files or migrated sessions.
It still needs the system runtime libraries used by the build.

For the first installation only, the script copies the app-specific directory
`build/profile/config/Notepadqq`. It includes preferences, `session.xml`,
`tabCache` and `backupCache`; unrelated KDE settings, recently used files,
font caches and GPU caches are not imported. Session cache references inside
the imported profile are adjusted to their destination, including recovery
sessions in `backupCache/window_*/window.xml`. Document paths, tab titles,
colors and cached text are preserved. The source is never moved or deleted.
Invalid session XML aborts before installing files; profiles containing symbolic
links are rejected rather than following links during migration.

An existing destination profile is renamed to a sibling named
`Notepadqq.before-fork-<unique-id>`, whose path is printed. The migration is
staged before replacement and restores that backup if the final rename fails.
The installed profile contains `.alamios-user-install.json` to prevent a later
update from replacing newer sessions with the development copy. Do not remove
that marker unless you intend to import the development profile again.
If no development profile exists, the standard profile is retained.

Useful options (also listed by `./install-user.sh --help`):

- `--dry-run`: print paths and migration policy without building or writing.
- `--keep-profile`: retain the existing standard profile on the first install.
- `--profile-source /path/to/profile`: import another development XDG profile
  root containing `config/Notepadqq`.
- `--preset dev`: install a debug build instead of the default release build.
- `--prefix /path/to/prefix` and `--jobs 4`: override destination and parallelism.

`XDG_CONFIG_HOME` and `XDG_DATA_HOME` are respected. With a custom data home,
the menu launcher and icons also go there. No shell startup files, file
associations or package-manager installations are changed. Menu caches are
refreshed when the relevant tools are available. The installer requires
Python 3, `pgrep`, CMake and the normal build dependencies. It installs local
source changes and never fetches from Git. Future updates use the same command:

```sh
./install-user.sh
```

For terminal use, ensure `~/.local/bin` precedes `/usr/bin` in `PATH`; otherwise
use `~/.local/bin/notepadqq` explicitly. The user desktop entry shadows an apt
entry with the same `notepadqq.desktop` ID, but does not uninstall apt. Flatpak,
AppImage and the old development launcher may still have separate menu entries.
Remove those separately through their package manager or Plasma's menu editor,
after checking the migrated sessions. Re-pin the new **Notepadqq** entry if the
old taskbar pin targets the development launcher. Do not run apt and this fork
against the shared standard profile at the same time; all editions also share
the single-instance application identity.

Installer regression checks use temporary fixtures and mocked build commands:

```sh
python3 scripts/test_install_user.py
```

They cover unsaved text and recovery-cache path migration, previous-profile
backup, repeated updates, retaining the standard profile, fresh installs,
invalid XML, build failure, active-editor refusal and a write-free preview.
These checks do not install into the real user profile or refresh its menu.
All 10 installer regression tests pass. The actual existing `dev` build's
CMake payload was also installed into a temporary prefix and its rewritten
desktop entry passed `desktop-file-validate` (with existing upstream category
and deprecated-action warnings). The real user installation has not been run
as part of preparing this change.

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

The implemented fork features are available after building or running
`./update-fork.sh`; see the [README](README.md#implemented-fork-features). The
maintainer has tested a local menu launcher targeting `build/install/dev`. That
launcher is machine-local and is not created by the update script. A fresh
checkout can run the binary with the isolated profile below. No fork AppImage
or distribution package has been produced.

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
inside the development profile, including unsaved documents. The per-user
installer above deliberately uses the standard identity and profile to replace
normal use of upstream; it is not a mechanism for running both simultaneously.
Development builds should continue to use isolated storage.

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
