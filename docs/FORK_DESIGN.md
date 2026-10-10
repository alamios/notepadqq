# Initial fork design

Status: comment toggling implemented; tab and diff designs remain proposals.
Maintainer: **alamios**.
See [requirements](FORK_REQUIREMENTS.md) for user requests and acceptance criteria.

## Existing architecture

| Area | Existing code | Design implication |
| --- | --- | --- |
| Editor host | `src/ui/EditorNS/editor.cpp`, `customqwebview.cpp` | Qt WebEngine hosts the editor and carries input/events |
| Editor commands | `src/editor/app.js`, `classes/UiDriver.js`, `Messages.md` | Extend the existing message bridge rather than add a parallel API |
| Engine | `src/editor/index.html`, `libs/codemirror` | CodeMirror is the default; optional Monaco exists behind `NQQ_USE_MONACO` |
| Tabs | `src/ui/editortabwidget.cpp` | `QTabWidget` currently enforces a 100 px minimum width and 24 px height on Linux |
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
Only the public original URL is configured as `upstream`; no hosted fork or
push has been performed.

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
`ctest --preset dev --timeout 60` pass with Qt 6.10.2. The UI suite has 23 passing
results, including 17 comment scenarios and an actual keyboard-shortcut test;
the C++ correctness suite has 31 passing results. JavaScript syntax, UI XML,
documentation links, and `git diff --check` were also verified. Manual desktop
appearance and user shortcut preferences remain outside this automated review.

## Tab presentation and metadata

Implement compact sizing first by replacing the hardcoded style dimensions with
settings for tab density and maximum title width. Preserve full titles/tooltips,
close buttons, and the existing dirty icon.

Store a color identifier on the document's presentation metadata, independent
of tab index. Persist it as an optional session attribute; missing/invalid
values mean the default palette. Extend the move/clone policy and restoration
explicitly so colors follow the document. Use a small theme-aware palette and
the existing context-menu integration rather than coloring file contents.

Multiple rows require a dedicated tab-strip layout: changing a stylesheet alone
does not create wrapping tabs. Prototype a Qt widget that owns visual rows while
the logical document list remains flat. Keep one authoritative mapping from
document identity to logical index. Route selection, close, drag/drop, and
context menus through that mapping. Adapt `EditorTabWidget` and
`TopEditorContainer`, which currently query `QTabBar::tabAt()` and related
signals. Avoid independent document stacks per row. Single-row mode remains
available and uses the same document model.

Test row layout/resize, cross-row reordering, keyboard navigation, active/dirty
indicators, color persistence, clone/move behavior, long names, and scaling.

## Two-document differences

Reuse the existing split panes, but add a comparison controller that associates
two live document identities and listens to edits, closure, and view movement.
Keep documents editable. A minimal entry/exit action chooses the pair; no
navigation, merge, or staging controls are requested.

Capture in-memory text through the existing bridge, then calculate line-based
diff hunks in a worker. Start with a pure line-diff implementation (for example,
Myers with explicit work/size limits); choose a library only after checking
maintenance, license, and dependency impact. Debounce edits and attach revision
tokens to snapshots so stale results are discarded. Large/pathological inputs
must cancel or fail gracefully without monopolizing the UI or worker.

Apply red removed-line decorations on the left and green added-line decorations
on the right via CodeMirror line classes/markers. Render colors with readable
text in light and dark themes; preserve syntax highlighting. Track only
comparison-owned decorations and remove those on exit. No document-text edits
or synthetic blank lines are allowed for presentation. Scroll synchronization
and visual alignment spacers are deferred unless the simple presentation proves
insufficient; they are not prerequisites for the requested highlighting.

Normalize line endings for comparison while retaining a distinct final-newline
state. Test insertions, deletions, replacements, duplicate lines, empty inputs,
Unicode, unsaved changes, updates arriving out of order, and document closure
while a diff is pending. Compare the exact expected hunk ranges in pure tests,
then verify both panes and decoration cleanup in Qt/WebEngine integration tests.

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
