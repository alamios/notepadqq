# Packaging and installation

## Per-user installation

Close every Notepadqq instance and run `./install-user.sh`. The script builds
`release` with four parallel jobs and installs the current checkout without
`sudo` or fetching source updates.

| Component | Default path |
| --- | --- |
| Executable | `~/.local/bin/notepadqq` |
| Editor assets | `~/.local/share/notepadqq` |
| Desktop entry | `~/.local/share/applications/notepadqq.desktop` |
| Icons, metainfo and manual | `~/.local/share` |
| Settings, sessions and document caches | `~/.config/Notepadqq` |

The application name is **Notepadqq** and the desktop ID is `notepadqq.desktop`.
All launcher actions use the absolute installed executable, with `Terminal=false`.
The entry can be pinned in Plasma without relying on shell `PATH` configuration.
For terminal use, put `~/.local/bin` before `/usr/bin` in `PATH`, or invoke the
installed executable explicitly. No shell startup files or file associations
are modified.

Installed files and migrated sessions do not depend on the checkout location.
System runtime libraries, including Qt, remain required. This installer does
not produce a self-contained AppImage or a distribution package.

## Sessions and updates

On the first install, `build/profile/config/Notepadqq` is copied if it exists.
Preferences, `session.xml`, `tabCache` and `backupCache` are included; unrelated
KDE settings, recent-file lists and GPU/font caches are excluded. Cache references
inside session/recovery XML are rewritten for the destination. Document paths,
colors, titles and cached contents remain intact. The source profile is retained.

An existing destination is backed up beside it as
`Notepadqq.before-fork-<unique-id>`; the path is printed. Migration is staged
before replacement, with rollback if the final rename fails. Invalid session
XML or symbolic links in the source abort before installing files.

`.alamios-user-install.json` marks the installed profile so later updates do
not import older development sessions. Keep that marker. If no development
profile exists, the standard profile is retained. Use `--keep-profile` to retain
it explicitly on the first installation.

After obtaining new source changes, close Notepadqq and rerun:

```sh
./install-user.sh
```

## Installer options

- `--dry-run`: show destinations and migration policy without building or writing.
- `--keep-profile`: skip importing the development profile.
- `--profile-source /path/to/profile`: use an XDG profile root containing
  `config/Notepadqq` instead of `build/profile`.
- `--prefix /path/to/prefix`: change the installation prefix.
- `--preset dev`: install debug instead of the default release build.
- `--jobs 4`: change build parallelism.

`XDG_CONFIG_HOME` and `XDG_DATA_HOME` are respected. A custom data home receives
the launcher and icons. Available desktop-cache tools are invoked after install;
if menu refresh fails, log out and back in. Installer requirements are listed
in the [README](README.md#build-and-install).

The user desktop entry overrides an apt entry with the same desktop ID. Other
packages are not uninstalled, and Flatpak, AppImage or development launchers may
have separate entries. Remove them with their package manager or Plasma's menu
editor after checking the migrated session. Re-pin the new **Notepadqq** entry
if the previous pin points to the development launcher. Do not run multiple
editions against the shared profile or single-instance identity.

## Development installation

`./update-fork.sh` builds `dev` and installs into `build/install/dev`. It
preserves the repository profile and an existing development launcher, but does
not create one. Use [isolated development profiles](docs/DEVELOPMENT.md#isolated-profiles)
for experiments; use `install-user.sh` for normal daily-use installation.

## System packages

CMake installs the real executable as `bin/notepadqq`, assets under
`share/notepadqq`, a desktop entry, AppStream metadata, icons and a manual page.
The preset's default prefix is repository-local, so explicitly choose the
system prefix when packaging. For example, after building `release`:

```sh
DESTDIR="$PWD/build/package-root" cmake --install build/release --prefix /usr
```

This stages a package under `build/package-root`; it does not install into the
live system. Package maintainers are responsible for runtime dependencies and
any format-specific wrappers. The existing `snap/` recipes are inherited
packaging infrastructure; they do not establish a tested fork release.
