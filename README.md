# <img src="https://user-images.githubusercontent.com/4319621/36906314-e3f99680-1e35-11e8-90fd-f959c9641f36.png" alt="Notepadqq" width="32" height="32" /> Notepadqq [![Build Status](https://github.com/notepadqq/notepadqq/actions/workflows/build-test.yml/badge.svg)](https://github.com/notepadqq/notepadqq/actions/workflows/build-test.yml) [![notepadqq](https://snapcraft.io/notepadqq/badge.svg)](https://snapcraft.io/notepadqq)

> [!NOTE]
> This fork is actively maintained by **alamios**. Development focuses on
> comment shortcuts, compact colored tabs with multiple rows, and side-by-side
> document differences.
>
> Upstream has reported unexpected crashes with recent OS/Qt versions. This
> warning does not establish a reproduced defect in this fork. Track any
> encountered errors in the [bug log](docs/BUG_LOG.md).

### Links

* [Fork scope and requested changes](docs/FORK_REQUIREMENTS.md)
* [Fork implementation design](docs/FORK_DESIGN.md)
* [Bug log and stability observations](docs/BUG_LOG.md)
* [What is it?](#what-is-it)
* [Build it yourself](#build-it-yourself)
* [Download it](#distribution-packages)

### Local fork

This checkout is the starting point for a Linux desktop fork maintained by
**alamios**, focused on comment shortcuts, compact colored tabs with multiple
rows, and side-by-side document differences. Source code, comments,
and project documentation are maintained in English.

The `upstream-baseline` tag records commit
`37652c5efa5fe8720f4eaf9f2e459888c1c6d6d0`, before fork changes.
That commit declares development version **2.2.0**; it is not the 2.1.1 release.
Fork work starts on `fork/development`. The `upstream` remote points to
`https://github.com/notepadqq/notepadqq.git`; a hosted fork and its `origin`
remote have not been configured yet.

The linked requirements and design distinguish implemented features from planned
work. Existing distribution packages do not include fork changes.
The user reports working scrolling and responsive preferences in version 2.1.
Session restoration is assumed to be fixed for now, without a verified recovery
test. These older bug reports do not block feature work; new or recurring errors
are recorded in the bug log before fixes are proposed.

#### Implemented fork features

**Toggle Comment** is available under **Edit**, with default shortcut
`Ctrl+Shift+C`. Change the binding in **Settings > Preferences > Shortcuts**.
An empty selection toggles the current line; full-line selections toggle those
lines. Partial selections use block comments where the language supports them,
otherwise line comments. Indentation and selection direction are preserved,
and each toggle is a separate undo step. Unsupported languages and read-only
documents are left unchanged.

This action currently supports the default CodeMirror backend. It is disabled
for Monaco. The user has confirmed that comment toggling works in the installed fork.

**Compact tabs** are enabled by default. Adjust **Compact tabs** and **Maximum
compact tab width** in **Settings > Preferences > Appearance**. The default
maximum is 160 logical pixels; long titles are elided in the middle, with the
full title or path available in the tooltip. Close buttons and dirty indicators
remain visible. Disable compact mode for wider tabs.

Right-click a tab and choose **Tab Color** to assign red, orange, yellow, green,
blue, or purple, or **Default** to remove its color. A colored band keeps native
text and active-tab styling readable. Colors follow reordered and moved tabs
and are stored in sessions and automatic backups, including unsaved documents.
Changing a color does not mark document contents as modified.

**Titles for unsaved documents:** right-click an unsaved tab and choose **Set
Tab Title...**. Enter a title to replace its automatic name; use the same action
to change it again. Canceling or entering a blank title leaves the name unchanged.
The title follows the tab when moved and survives session restoration and
automatic backups, including empty titled tabs. Naming a tab changes no text
or undo history. Saving the document uses the real filename as its tab title;
saving a copy keeps the unsaved document's title.

Multiple-row tabs and document comparisons remain planned. Tests use temporary
settings and the actual Qt/WebEngine editor; a manual Plasma review of the new
tab appearance remains pending.

#### Development setup on Kubuntu

To rebuild and reinstall the current checkout, close the fork and run:

```sh
./update-fork.sh
```

The script locates the repository relative to its own file, so it also works
when invoked from another directory. It configures the `dev` preset, builds
with four parallel jobs, and installs into `build/install/dev` without `sudo`.
It preserves `build/profile` and the existing menu launcher. It uses local
source changes and does not fetch or pull from Git.

The development build now configures and compiles with Qt 6.10.2 on Kubuntu
26.04. The initial attempt was blocked by missing Ninja and development
dependencies; those packages are now available. For a fresh setup, install:

```sh
sudo apt install ninja-build qt6-base-dev qt6-5compat-dev qt6-tools-dev \
    qt6-tools-dev-tools qt6-webengine-dev qt6-websockets-dev \
    qt6-webchannel-dev qt6-svg-dev libuchardet-dev
```

Then use the existing `dev` build and test presets below. Development builds
must use separate configuration and session storage, as described in
[Packaging Notes](PACKAGING.md#fork-development-isolation).

#### What is it?

Notepadqq is a text editor designed by developers, for developers. 

![screenshot_20180302_163505](https://notepadqq.com/s/images/snapshot1.png)

Please visit our [Wiki](https://github.com/notepadqq/notepadqq/wiki) for more screenshots and details.

Build it yourself
-----

| Build dependencies    | Dependencies      |
|-----------------------|-------------------|
| Qt 6.4 or higher      | Qt 6.4 or higher  |
| qt6-webengine5-dev    | qt6-webengine5    |
| qt6-websockets-dev    | qt6-websockets    |
| qt6-svg-dev           | qt6-svg           |
| qt6-tools-dev-tools   | coreutils         |
| libuchardet-dev       | libuchardet       |
| pkg-config            |                   |


#### Get the source

    $ git clone --recursive https://github.com/notepadqq/notepadqq.git
    $ cd notepadqq

#### Build

    notepadqq$ cmake --preset release
    notepadqq$ cmake --build --preset release

To build with debug symbols, use the `dev` preset instead:

    notepadqq$ cmake --preset dev
    notepadqq$ cmake --build --preset dev

If you encounter errors make sure to have the necessary libraries installed. For Ubuntu you can do that using apt-get:

    sudo apt-get install qt6-tools-dev qt6-tools-dev-tools qt6-webengine-dev qt6-websockets-dev libqt6svg6 libqt6svg6-dev libuchardet-dev pkg-config

For CentOS:

    sudo dnf install -y qt6-qtbase-devel qt6-qttools-devel qt6-qtwebengine-devel qt6-qtwebsockets-devel qt6-qtsvg-devel qt6-qtwebchannel-devel uchardet pkgconfig

Building for **macOS**? Check [here](https://github.com/notepadqq/notepadqq/wiki/Compiling-Notepadqq-on-macOS).

#### Run tests

    notepadqq$ ctest --preset release

#### Install

You can run notepadqq from its build output folder. If however you want to install it, first build it
by following the above steps, then run:

    notepadqq$ sudo cmake --install build/release

#### Qt

If the newest version of Qt isn't available on your distribution, you can use the [online installer](http://www.qt.io/download-open-source) to get the latest libraries and install them into your home directory (`$HOME/Qt`). Notepadqq will automatically use them.

Distribution Packages
---------------------

#### Ubuntu, Debian, and others:

    sudo apt install notepadqq

#### Snap

To install the latest stable version:

    sudo snap install notepadqq

You don't have the `snap` command? Follow the instructions at https://docs.snapcraft.io/core/install and then install Notepadqq as shown above.

You can follow the unstable development releases from the "edge" channel.

#### Arch Linux (community-maintained)
Notepadqq is available from Arch's [community repositories](https://www.archlinux.org/packages/community/x86_64/notepadqq/). To install using pacman:

    sudo pacman -S notepadqq

Alternatively it can be found in the AUR:

 * Development (git version): [notepadqq-git](https://aur.archlinux.org/packages/notepadqq-git/)

#### OpenSUSE (community-maintained)
Notepadqq is avilable in OpenSUSE's main repository:

     sudo zypper in notepadqq

#### Solus (community-maintained)
Notepadqq is available in the `shannon` (stable) repository:

     sudo eopkg it notepadqq

#### Others
Use a package for a compatible distribution, or build from [source](https://github.com/notepadqq/notepadqq.git).
If you want to submit a package: https://github.com/notepadqq/notepadqq-packaging
