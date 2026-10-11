# Initial fork design

Status: comments, compact/full-title tabs, colors, unsaved titles and multiple-row
tabs and insertion previews implemented; telemetry removed. Document differences
are implemented with temporary read-only documents.
Maintainer: **alamios**.
See [requirements](FORK_REQUIREMENTS.md) for user requests and acceptance criteria.

## Existing architecture

| Area | Existing code | Design implication |
| --- | --- | --- |
| Editor host | `src/ui/EditorNS/editor.cpp`, `customqwebview.cpp` | Qt WebEngine hosts the editor and carries input/events |
| Editor commands | `src/editor/app.js`, `classes/UiDriver.js`, `Messages.md` | Extend the existing message bridge rather than add a parallel API |
| Engine | `src/editor/index.html`, `libs/codemirror` | CodeMirror is the default; optional Monaco exists behind `NQQ_USE_MONACO` |
| Tabs | `src/ui/editortabwidget.cpp` | Native `QTabWidget` with a compact `QTabBar` subclass and document color bands |
| Split panes | `src/ui/topeditorcontainer.cpp`, `editoruicontroller.cpp` | Reuse existing horizontal splitter and move/clone behavior |
| Preferences | `src/ui/frmpreferences.cpp`, `include/nqqsettings.h` | Reuse settings and shortcut conflict detection |
| Sessions | `src/ui/Sessions/sessions.cpp`, `persistentcache.cpp` | XML stores views/tabs and references cached modified content |
| Recovery | `src/ui/Sessions/backupservice.cpp`, `src/ui/main.cpp` | Preserve existing recovery; investigate only if a defect recurs |
| Tests | `src/ui-tests/tst_notepadqqtest.cpp`, `tst_cppcorrectness.cpp` | Extend existing Qt test targets for relevant regressions |

Use the default CodeMirror build for the first milestones. Do not silently
advertise new engine features as supported on Monaco: document and test that
backend separately if it becomes part of the delivered scope.

## Baseline and isolated development

The checkout was clean before tagging. `upstream-baseline` points to
`37652c5efa5fe8720f4eaf9f2e459888c1c6d6d0`; `fork/development` starts there.
The initial checkout recorded the public original URL as `upstream`. The
maintainer now manages the hosted fork, remotes and publication.

First configure/build with the `dev` preset and run existing CTest targets.
The initial configure failed because Ninja is missing. Package inspection also
found missing Qt development packages and `libuchardet-dev`; the C++ compiler,
CMake, Make, pkg-config, Node, and npm are present. The README lists the setup
command. The dependencies are now available; the isolated development build
compiles with Qt 6.10.2. Both existing CTest targets passed before feature edits.
WebEngine tests require execution outside the agent's restricted process/socket
sandbox; their settings remain isolated in a temporary directory. No packages
have been installed by the agent.

Use repository-local XDG directories and close other Notepadqq instances before
running GUI tests. `SingleApplication::socketNameForUser()` uses a fixed ID;
the optional D-Bus path also uses a fixed service name. Consequently, separate
storage is not sufficient for concurrent fork/upstream instances. Before
shipping the fork, give it a distinct application/desktop/IPC identity and make
test instance identity profile-aware. Choose public branding later.

## Stability policy and session scope

The user reports working scrolling and no preference hangs in version 2.1.
Session restoration is assumed fixed for now by user direction; this has not
been verified by a recovery test. These observations are against a packaged
release, not an automated validation of the development checkout.

Focus implementation on the requested features. Do not make reproduction of
historical defects, a recovery lifecycle rewrite, persistent window UUIDs, or
snapshot-generation migration prerequisites. Keep the existing recovery
behavior and XML/cache format. Adding optional tab-color metadata still needs
save/load and backup-path tests with old sessions and missing/invalid values.
Any document presentation identity introduced for tabs or comparison must not
require a new recovery protocol.

Record concrete new or recurring errors in [the bug log](BUG_LOG.md), including
build/Qt/environment details and reproduction evidence. The upstream warning
about unexpected OS/Qt crashes remains a reason to track incidents, not proof
of a current fault. Confirmed regressions can interrupt feature work according
to severity; speculative fixes are not scheduled.

Historical source inspection found address-based backup window directories,
restoration of all backup subdirectories, process-local cleanup bookkeeping,
and deletion/recreation of content caches before XML replacement. These are
code observations, not confirmed defects in the current build. Revisit them
only when a reproduction makes them relevant.

## Comment action

The vendored CodeMirror `addon/comment/comment.js` implements line comments,
block comments, and uncommenting. The fork now loads that addon and exposes
`C_CMD_TOGGLE_COMMENT` through an Edit-menu `QAction`, the existing active-editor
controller, and shortcut settings. The message is documented in
`src/editor/Messages.md`. The action is disabled for the Monaco backend.

`Ctrl+Shift+C` is the configurable default. The command consults the active
mode's comment metadata, including embedded modes. Line-based selections use
line toggle; partial selections use block syntax where available. Overlapping
line targets are merged before edits, including mixed inline/line targets.
All changes run inside one CodeMirror operation with explicit history boundaries.
Read-only documents are unchanged.

Temporary bookmarks preserve non-empty selections around inserted delimiters
and retain selection direction. Restored selections are copied into a new array
before calling `setSelections`; modifying CodeMirror's live range array would
corrupt its internal selection objects. Exact inline delimiters are removed
without accidentally uncommenting another block on the same line. Qt tests
exercise menu-action dispatch, real key events, comment/uncomment round trips,
undo/redo boundaries, unsupported languages, and read-only behavior.

Validation: `cmake --build --preset dev -j 4` and
`ctest --preset dev --timeout 60` pass with Qt 6.10.2. The UI suite has 88 passing
results, including 17 comment scenarios and an actual keyboard-shortcut test;
the C++ correctness suite has 28 passing results. JavaScript syntax, UI XML,
documentation links, and `git diff --check` were also verified. Manual desktop
appearance and user shortcut preferences remain outside this automated review.
The user has confirmed that comment toggling works in the installed fork.

## Tab presentation and metadata

Tab widths are measured from the full text, icon and close-button sizes, with
explicit padding. `minimumTabSizeHint()` uses the same size so native styles
cannot shrink tabs until even short names are elided. Compact density is enabled
by default. `Appearance/ShowFullTabTitles` defaults to true: names use natural
width without a cap. When it is false, compact widths are capped by
`Appearance/MaximumTabWidth` (160 logical pixels by default, clamped to 96–320).
The width control is enabled only for compact, elided titles. Preferences update
all open panes immediately. Native labels are painted below the color band,
leaving a gap before icons; dirty indicators and close buttons retain native
rendering and hit testing.

`Editor::tabColor()` holds a validated color identifier independently of tab
indices and titles. Six colors plus Default are available in the tab context
menu. The menu captures a guarded document pointer. The tab bar draws a color
band above the native label, preserving theme text, focus and selection styles.
`tabData` follows native reordering; explicit signal connection/disconnection
and metadata initialization keep moved documents associated with their color.
Color changes do not alter text, undo history or the document's dirty state.

Sessions store non-default colors in an optional `tabColor` XML attribute.
Missing or unknown values restore the default. The loader applies the color
as soon as the editor is reserved, before asynchronous content loading finishes.
The same format serves session saving and cached-document backups. Backup
change detection must include color metadata even when text history is unchanged.
This does not change recovery directories or cleanup policy.

Tests cover live compact preferences, long names/tooltips, close-button presence,
color painting, reordering and pane transfers, context-menu dispatch, clean-file
sessions, modified-file and unsaved-document backups, legacy XML, invalid colors,
and automatic backup updates after color-only changes. Both CTest targets pass
(88 UI results and 28 C++ correctness results). Native Plasma/Wayland
appearance remains a manual check. Upstream's clone-view action has no handler
in this baseline; this milestone introduces no new clone behavior.

### Unsaved tab titles

`Editor::unsavedTitle()` stores an optional user title for documents without a
file path. The setter trims whitespace and ignores blank values or saved
documents. It updates the canonical tab name before emitting
`unsavedTitleChanged`, allowing the tab widget and active window to refresh
without altering content or history. Transfers retain the document and reconnect
its presentation signals. Setting a real file path clears the custom title;
Save Copy retains it.

The context menu adds **Set Tab Title...**, disabled for saved documents. A
guarded document pointer survives the input dialog's nested event loop. Sessions
store optional `unsavedTitle` metadata and apply it both immediately and after
asynchronous loading. Automatic backup change detection also includes the title.
Titles are never used as cache filenames: named unsaved documents use a generated
`untitled` cache basename, so slashes, Unicode, XML characters and duplicate
titles are safe.

Named clean empty documents are cached too. The optional `cachedClean="1"`
attribute preserves their clean state on restore; it is honored only for named
unsaved documents. Older sessions default to the existing cached-dirty behavior.
This adds metadata to the existing session format without changing the recovery
lifecycle. Tests cover accepted/canceled dialogs, saved-file disablement,
reordering, transfers, Save/Save Copy, metadata-only backup updates, and duplicate
arbitrary titles on dirty and clean-empty session round trips.

### Multiple-row tabs

`Appearance/WrapTabs` defaults to true. `EditorTabBar` remains the authoritative
flat `QTabBar` used by `QTabWidget`; child `TabRow` bars present successive ranges
of that model. Width changes greedily reflow rows in logical order. Size hints
reserve the combined row height above the document stack, avoiding independent
editor stacks per row. A single oversized title gets scrolling within its row.
Single-row mode keeps the native bar and scroll buttons.

Selection, keyboard traversal, wheel selection, close buttons, middle clicks,
context menus and double clicks map row-local positions back to logical indices.
Cross-row drag release calls the authoritative bar's `moveTab`, so Qt reorders
its associated document stack. A custom selected state draws exactly the active
logical tab; inactive rows do not show a second selected document. Rebuilds are
queued and reuse row widgets to avoid destroying an event source during signals.
`EditorTabWidget::visualTabRect` and `tabAtPosition` expose the visual mapping;
`TopEditorContainer` uses it for context menus. Document titles, colors, sessions
and pane transfers retain the existing flat model.

Regression tests check exact capped widths, readable short titles, default full
names, live preferences, resize/reflow, mapped clicks, keyboard traversal,
closing, context menus and dragging between rows. An offscreen preview is saved
as `build/dev/tab-rows-preview.png`. Actual Plasma/Wayland drag behavior and native
styling still need desktop validation.

### Tab insertion preview

Both single-row and wrapped dragging use the same `dropTarget` calculation.
Each visible row maps its tab midpoints to insertion boundaries in the flat
model, including its beginning/end and empty trailing space. The destination
index is corrected for removal of the source tab. Qt's immediate single-row
reordering is disabled so no model mutation occurs during the preview.

`TabInsertionMarker` is a four-pixel overlay in the application highlight color.
It is mouse-transparent and raised above row widgets, including after queued
row rebuilds. `TabRow` forwards moves after the platform drag threshold; the
authoritative bar handles single-row moves directly. Both release paths call
the same boundary calculation, hide the overlay and then reorder the Qt model.

Escape is intercepted only while dragging, including when tab selection moves
keyboard focus to the editor. Outside drops, settings changes, resize and model
insertion/removal clear the preview and pending press state. No new preference
or translated label is needed.

Twelve regression cases cover before/after, forward/backward movement, row
trailing space, unchanged positions, outside drops, cancellation with editor
focus, single-row mode and right-to-left layout. Tests verify marker geometry
and pixels in the whole strip capture as well as final document identity/order.
The preview artifact is `build/dev/tab-drag-preview.png`. The user has accepted
the installed drag behavior on Plasma.

### Translations

All 14 existing Qt translation catalogs include the new comment action/tooltip,
tab colors, unsaved-title dialog and appearance controls. Source strings and
project documentation remain English. UI tests load each compiled `.qm` file
and verify every new entry; the Spanish catalog is also installed in a test to
check Qt's runtime lookup. Building `ui-tests` depends on translation generation.

## Telemetry removal

The maintained fork removes upstream telemetry entirely. `stats.cpp`,
`statsruntime.cpp` and their headers no longer exist or appear in CMake. Startup
and preference application no longer initialize telemetry. The collection
checkbox, consent dialog and telemetry-only translation entries are removed.
The legacy collection, transmission timestamp and consent-state settings are
removed during startup compatibility handling, even when no version migration
is otherwise needed. No other preferences or session storage are changed.

The three upstream telemetry-only correctness tests and their fake network
fixtures are removed along with the code they exercised. A regression test
checks removal of legacy opt-in keys while retaining unrelated preferences;
the preferences integration test verifies the checkbox is absent. Source and
binary checks verify that the upstream telemetry endpoint is absent. Qt Network
remains necessary for unrelated editor/extension functionality.

The maximum-width preference now sits directly below the full-title checkbox.
Tab height padding is reduced by four logical pixels in both density modes;
label painting retains the reserved band area. The existing row-preview test
covers the resulting rendering without additional tests for the layout-only
change.

## Two-document differences

Implemented for CodeMirror. `ComparisonController`, owned by the editor
container, associates guarded live document identities. It transfers the pair
into the first two existing split panes and selects the reference on the left
and compared document on the right. Other tabs remain available; tab indices
and names are not used as document identity. One pair is active per window.

`Compare with...` is a View action shared by the tab context menu and toolbar.
Its modal picker lists other open editors with pane/tab positions and optional
paths, handling duplicate titles. The shared comparison action is checkable:
while active, it uses the existing translated `End Comparison` label/tooltip
and invokes stop directly, including from the toolbar. Canceling the picker
leaves it unchecked. It remains available to stop even if the selected tab is
unrelated to the pair. Shortcuts use the existing preferences machinery,
without assigning a potentially conflicting default. A bundled SVG supplies
the toolbar icon. Existing custom toolbars get the new action once using the
internal `MainWindow/ComparisonToolbarAdded` setting; future customization is
respected.

The user changed the initial editable proposal on 2026-10-11: comparison now
locks both documents temporarily. `C_CMD_DIFF_LOCK` retains the exact previous
CodeMirror read-only option and enables read-only mode. `C_CMD_DIFF_END` clears
only comparison decorations and restores that option. A `beforeChange` handler
blocks programmatic text changes as well; menu transformations return before
moving selections. Undo, redo and history clearing are guarded before entering
CodeMirror, because canceling an undo in `beforeChange` would discard history.
The lock is presentation state and is not serialized in sessions or backups.

Snapshots use the asynchronous bridge and include an immediately incremented
JavaScript revision. A 200 ms timer coalesces pending change events. An owned
cancellation token and controller generation invalidate pending snapshot/worker
results on restart, exit or destruction. Applying marks validates the JavaScript
revision again. The worker touches only captured strings and its atomic token;
Qt delivers completion on the GUI thread and deletes the worker after finish.
No synchronous text retrieval or UI-thread diff calculation is used.

`LineDiff::compare` computes line changes using a bounded longest common
subsequence table after trimming equal prefixes/suffixes. It normalizes CRLF/CR
without modifying documents and keeps final-newline state via a trailing empty
line. Limits: 4 Mi UTF-16 code units and 100,000 lines combined, 20,000 changed
region lines, and 4,000,000 table cells (at most 16 MB for the int table).
Cancellation is checked during calculation and reconstruction. Exceeding a
limit exits comparison, restores editing, and reports the reason in the status
bar. Identical documents also get a status message.

CodeMirror background line classes render translucent red removed/replaced
lines and green added/replaced lines without replacing syntax colors. Cleanup
tracks owned line handles/classes; it does not alter text, history or the dirty
state. Closing a member, transferring a member outside the window, or placing
both members in one pane ends comparison. Movement validation is deferred until
transfers finish. Selecting unrelated tabs preserves the pair. The window
stops comparison before destroying the editor UI controller.

Each start records guarded original pane identities, pane indices and tab
indices, after stopping/restoring any previous pair. Stop clears the active
pair before transferring surviving documents home and restores tab order in
ascending original-index order. Missing original panes are recreated at their
previous splitter positions; closed documents are not recreated and documents
moved to another window are not transferred back. Only a pane actually created
by the comparison is removed, and only if empty. Existing or newly populated
panes remain intact. The reference is selected on return; the container's
`activateTabWidget` updates its active-pane pointer before removing a temporary
pane. A guarded container prevents restoration during parent destruction.
Origins and the temporary-pane reference are reset after each stop.

Twelve pure diff cases cover additions, removals, replacements, duplicate/blank
lines, empty inputs, Unicode, line endings and final newlines. Integration tests
exercise the actual picker/toolbar action, existing-toolbar migration, document
identity, colors/CSS, stale revision rejection, editing and history locks,
restoration of prior read-only state, closure, pane transfers, pending worker
cancellation and controller destruction. Limit/cancellation tests include large
mostly-equal input. All 14 locale catalogs contain the new strings and are
checked after compilation. The offscreen artifact is
`build/dev/comparison-preview.png`. The user confirms the initial comparison
works on Plasma. Eight additional regression cases cover shared and swapped
origins, existing second/third panes, tab order/metadata, repeated stop/start,
missing origin panes, cross-window movement and parent destruction. The actual
toolbar button is clicked to stop in the UI test. Native Plasma confirmation
of the latest toggle/restoration refinements remains pending.

Independent scrolling is intentional in this first version. Alignment spacers,
scroll synchronization, character-level highlights, navigation and merge/staging
controls are outside the delivered scope. Monaco comparison is disabled.

## Verification and first implementation milestone

Documentation-only updates do not require new runtime tests. Once dependencies
are available, configure/build the isolated baseline and run the existing tests.
Then implement the comment action as the first feature, followed by compact and
colored tabs, multiple-row tabs, and red/green document comparison.

Add focused behavioral tests for each delivered feature. Keep manual
Plasma/Wayland checks for real keyboard/wheel input, tab geometry, colors, and
comparison panes. Tab-color persistence needs session round-trip and backup
coverage, but does not require replacing the recovery lifecycle.

Record errors as they are encountered. Reopen older bug reports only if symptoms
recur; do not delay feature work to investigate presumed upstream fixes. Update
the README, requirements, design, and bug log with actual implementation and
validation results.
