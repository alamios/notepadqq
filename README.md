# Notepadqq — alamios fork

A Linux text editor maintained by **alamios**, based on
[Notepadqq](https://github.com/notepadqq/notepadqq). It provides syntax
highlighting for more than 100 languages, multiple selections, configurable
shortcuts and session restoration, with additional tab and comparison features.
This fork contains no telemetry.

The current fork version is **2.3.0**. The About dialog identifies this fork
and credits **alamios** as its maintainer separately from Daniele Di Sarli and
the original Notepadqq contributors.

## Features

- Toggle line and block comments with `Ctrl+Shift+C`.
- Compact tabs with full titles, optional width limits and multiple rows.
- Tab colors, custom titles for unsaved documents and drag insertion previews.
- Side-by-side document comparison with red/green line highlights.
- A per-user installer with a normal **Notepadqq** menu entry and session migration.

See the [feature guide](docs/FEATURES.md) for controls and limitations. Comment
and comparison features use the default CodeMirror editor; they are disabled
with the optional Monaco backend.

## Build and install

Build requirements: a C++20 compiler, CMake 3.24 or newer, Ninja, Make,
pkg-config, Qt 6.4 or newer and libuchardet. Qt modules include Core5Compat,
DBus, LinguistTools, Svg, WebChannel, WebEngine, WebSockets and Widgets.
The user installer also needs Python 3 and `pgrep` (procps).

On Ubuntu/Kubuntu, install the development dependencies:

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
    libqt6core5compat6-dev libqt6svg6-dev libqt6websockets6-dev \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    qt6-webengine-dev libuchardet-dev python3 procps
```

Clone this fork and install it after closing all Notepadqq instances:

```sh
git clone https://github.com/alamios/notepadqq.git
cd notepadqq
./install-user.sh
```

The installer builds the release preset and installs into `~/.local` without
`sudo`. Open **Notepadqq** from the application menu and pin that entry to the
taskbar. The launcher opens without a terminal. Terminal use of `notepadqq`
requires `~/.local/bin` before `/usr/bin` in `PATH`.

On the first installation, an existing development profile in `build/profile`
is copied to the standard user profile, preserving settings and unsaved tabs.
Any previous destination profile is backed up. Use `--keep-profile` to retain
the standard profile instead, or `--dry-run` to preview the installation
without changes. Other installations and old launchers are removed separately.
See [packaging](PACKAGING.md) for paths, migration and installer options.

Distribution packages of the original Notepadqq do not contain this fork's
changes. The per-user installation needs system runtime libraries; it does not
bundle Qt.

## Update

After obtaining the source changes you want to install, close Notepadqq and run:

```sh
./install-user.sh
```

Subsequent installations retain the installed settings and sessions. The script
builds the current checkout, including local changes; it does not download
updates or modify shell startup files. Installed files and migrated sessions
are independent of the checkout's location.

## Development

```sh
cmake --preset dev
cmake --build --preset dev --parallel 4
ctest --preset dev --output-on-failure
python3 scripts/test_install_user.py
```

Use isolated profiles when running development builds. `./update-fork.sh`
installs a debug build into `build/install/dev` for the existing development
launcher; it does not update the normal per-user installation.
See [development](docs/DEVELOPMENT.md) for isolation and test instructions,
[architecture](docs/ARCHITECTURE.md) for implementation details, and
[contributing](CONTRIBUTING.md) for reports and pull requests.

## Project and license

The fork's source and issue tracker are at
[alamios/notepadqq](https://github.com/alamios/notepadqq). Code, documentation
and commit messages are maintained in English. The `upstream-baseline` tag
preserves the original starting commit; fork development targets `main`.

Notepadqq is licensed under [GPL-3.0](COPYING). Original copyright and
[third-party notices](src/ui/LICENSE-3RD-PARTY.txt) are retained.
