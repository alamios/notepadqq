# Architecture

## Application and editor bridge

The Qt Widgets application hosts the editor in Qt WebEngine. CodeMirror is the
default backend; Monaco is optional. `EditorNS::Editor`, `customqwebview.cpp`
and the JavaScript UI driver carry asynchronous commands and responses between
Qt and the editor. Fork bridge commands are documented in
[Messages.md](../src/editor/Messages.md).

Preferences use `frmPreferences` and `NqqSettings`. Appearance form labels have
explicit leading alignment so font controls and tab options share a consistent
edge across desktop styles. The font selector grows with the dialog while
numeric controls retain compact widths. Menu actions reuse the existing
shortcut machinery.

## Comments

The bundled CodeMirror comment addon provides language-aware line/block syntax.
`C_CMD_TOGGLE_COMMENT` is dispatched by the active-editor controller.
Line targets are merged before editing. Explicit history boundaries keep each
toggle separate, and bookmarks preserve selection direction around delimiters.
Unsupported modes and read-only documents are unchanged. Monaco disables the action.

## Tabs and sessions

`EditorTabBar` measures full text, icons and buttons for both preferred and
minimum size hints. Full titles use natural widths. Elided compact titles use
`Appearance/MaximumTabWidth`; `Appearance/WrapTabs` enables multiple rows.
Child `TabRow` bars display ranges of the authoritative flat tab model, mapping
input back to logical indices. `QTabWidget` continues to own the document stack.
Queued row rebuilds avoid destroying event sources during signal handling.

Drag previews and release use the same insertion-boundary calculation. The
four-pixel overlay uses the application highlight color and is transparent to
mouse input. No document reordering occurs until release. Escape, outside drops
and structural changes clear pending drag state.

Colors and unsaved titles belong to editor identities, not tab indices.
Session XML stores optional `tabColor` and `unsavedTitle` attributes; missing
or invalid colors use the default. `cachedClean="1"` preserves the clean state
of a named, empty unsaved document. Titles are never used as cache filenames.
Metadata changes participate in automatic-backup change detection even when
text is unchanged. These additions retain the existing XML/cache lifecycle.

## Comparison

`ComparisonController` owns one guarded document pair per window. It records
original panes and indices, transfers the pair to separate panes, and obtains
asynchronous content snapshots with revisions. Generation counters and an
owned cancellation token invalidate pending work on restart, exit or destruction.
The worker touches captured strings, never Qt widgets; results return on the
GUI thread and are checked against current revisions before applying marks.

`LineDiff::compare` trims equal edges, normalizes line endings and computes a
bounded longest common subsequence. Limits are documented in the
[feature guide](FEATURES.md). Exceeding them stops comparison and restores editing.
CodeMirror background classes provide red/green line highlights without
replacing syntax colors or changing text, dirty state or undo history.

Temporary locks retain the exact prior read-only state. Programmatic edits and
history operations are guarded as well as keyboard input. Locks and highlights
are presentation state and are not serialized into sessions.

Stopping clears the active pair before moving surviving documents home.
Original tab order is restored in ascending index order. Missing origin panes
can be recreated; documents moved to another window remain there. Only an empty
pane created specifically for comparison is removed. Window destruction and
closing a compared document clear controller state safely.

## Version and attribution

The application version comes from `POINTVERSION` and is used by the command
line and About dialog. CMake and packaging metadata use the same version.
The About dialog explicitly identifies the fork and separates its maintainer
and contributor links from the original project's authors, copyright and
contributor links. Original copyright attribution remains unchanged. Qt layouts
fit translated text and larger fonts, with translations in all 14 catalogs.

## Privacy

The upstream telemetry collection/transmission code, timers, consent dialog
and preference are removed. Startup compatibility handling deletes legacy
telemetry keys without changing other preferences. Qt Network remains for
editor/extension functionality; its presence does not imply telemetry.

## Installation

`install-user.sh` invokes the Python-standard-library installer in
`scripts/install_user.py`. CMake stages the binary and assets before publication
to the per-user prefix. Assets are located relative to the installed executable;
no repository path or runtime wrapper is required.

First-install migration copies only the development profile's Notepadqq
configuration directory. It rewrites `cacheFilePath` values rooted in that
source in `session.xml` and recovery `window.xml`, preserving document paths
and cached contents. Invalid XML and symbolic links abort migration. The profile
is prepared before installation and published using destination-filesystem
staging, with a backup/rollback rename for the previous profile. A marker prevents
later updates from importing stale development sessions.

The installer deliberately retains the standard Notepadqq application and
desktop identity. It replaces normal use of upstream rather than enabling
parallel editions. Paths and options are in [packaging](../PACKAGING.md).
