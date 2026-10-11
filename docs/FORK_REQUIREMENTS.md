# Fork requirements

Recorded: 2026-10-10. Status: comment toggling, compact tabs, tab colors and unsaved tab titles
implemented, including multiple-row tabs and insertion previews; telemetry is
removed. Read-only document comparison is implemented, including a toolbar entry.

## Purpose and baseline

Improve the existing Notepadqq editor for daily use on Kubuntu 26.04. Preserve
its language support, document editing, configurable shortcuts, tabs, and
restoration of saved and unsaved documents. Retain Qt and the current editor
engine instead of porting Notepad++ or replacing the engine.

The immutable starting reference is `upstream-baseline` at
`37652c5efa5fe8720f4eaf9f2e459888c1c6d6d0` (development version 2.2.0).
Development starts on `fork/development`, with the original repository recorded
as the original upstream. The maintainer manages fork remotes and publication.

All code and project documentation must be in English. Existing translations
remain supported. New work does not depend on plugins; removal of the existing
extension subsystem is outside this initial scope.

## Reported environment

- Flatpak 1.4 is usable but has missing scrollbars and non-working mouse-wheel
  scrolling, plus duplicate recovered sessions after unexpected termination.
- The apt package `2.0.0~beta1-4.1build1` hangs with high CPU consumption when
  preferences are changed; the user reports this with any preference change.
- The user reports that scrolling works and the application no longer hangs
  in version 2.1. This is user validation, not an automated test of this checkout.
- The reported recovery problem does not lose files: it restores the newest
  state and additional older copies with fewer recent documents.

At the user's request, session restoration is assumed fixed in version 2.1
for now; no crash-recovery test has confirmed it. Do not redesign recovery or
investigate the older bugs as a prerequisite to feature work. Record any new
or recurring error in [the bug log](BUG_LOG.md), then reproduce it before
changing production code. The upstream OS/Qt crash warning remains context,
not a confirmed current defect.

## Requested changes

| ID | Priority | Request | Current status |
| --- | --- | --- | --- |
| BUG-01 | Monitor | Visible scrollbars and working mouse-wheel scrolling | Reported resolved by the user in 2.1; no fork fix planned |
| BUG-02 | Monitor | Applying preferences must remain responsive | Reported resolved by the user in 2.1; no fork fix planned |
| BUG-03 | Monitor | Restore current sessions without older duplicate copies | Assumed fixed in 2.1 by user direction; unverified and deferred |
| EDIT-01 | P1 | `Ctrl+Shift+C` comments/uncomments lines and selected code blocks using the active language | Implemented for CodeMirror; automated tests pass; user confirms it works in the installed fork |
| TABS-01 | P1 | Smaller tabs | Implemented with configurable compact mode and maximum width; automated tests cover sizing and applying preferences; manual Plasma review pending |
| TABS-02 | P2 | Tabs wrap into multiple rows | Implemented and enabled by default; tests cover resize/reflow, logical hit testing, selection, closing, context menus and cross-row dragging |
| TABS-03 | P1 | User-selectable tab colors, preserved across restart | Implemented with six colors and reset in the context menu; session and backup coverage; manual Plasma review pending |
| TABS-04 | P1 | User-defined titles for unsaved documents | Implemented in the tab context menu, with session and automatic backup persistence |
| TABS-05 | P1 | Preview the insertion position while dragging a tab | Implemented and accepted by the user: a four-pixel accent line at the exact before/after boundary in single-row and wrapped modes; Escape/outside drops cancel |
| DIFF-01 | P2 | Compare two documents in parallel with Git-style red/green differences | Implemented for CodeMirror: picker, menu/tab-context/toolbar entry, temporary read-only panes, bounded background line diff and cleanup; comparison, toolbar toggle and original-pane restoration accepted by the user on Plasma; integrated in main on 2026-10-11 |
| PRIV-01 | P1 | Remove upstream telemetry from the maintained fork | Implemented: collection/transmission code, timers, consent UI, settings and telemetry-only tests removed; legacy opt-in keys cleared at startup |
| INSTALL-01 | P1 | Install as Notepadqq with a normal menu entry and persistent user data | Prepared: per-user release installer, one-time profile migration with backup, update preservation and write-free preview; 10 installer tests pass; real user installation left to the user |

Additional requests can be added with stable IDs when the user recalls them.

Follow-up tab requirements (2026-10-10): show full titles at natural width by
default, offer a checkbox to enable elision/max-width sizing, leave a gap between
color bands and icons, and translate every new option. These are implemented;
all 14 existing locale catalogs include the fork's new strings. Manual desktop
review remains pending after the user's width/spacing report.

Further refinements: place the maximum-width control immediately below the
full-title checkbox and reduce excess vertical spacing below the color band.
Both are implemented. The user has praised the multiple-row implementation;
the new height adjustment still needs native desktop confirmation.

## Existing behavior to preserve

Keep working scrollbar/wheel behavior and responsive preferences while adding
features. Preserve saved and unsaved documents, tab order, cursor/scroll state,
and existing session restoration. Test session metadata round trips when adding
tab colors, without making a recovery redesign part of that work.

## Feature acceptance criteria

### EDIT-01: comments

Use `Ctrl+Shift+C` as the default toggle-comment action, configurable through
the existing shortcut preferences. With no selection, toggle the current line;
with a line selection, toggle the selected lines. With a partial selection,
use block syntax when supported by the language, otherwise use line comments.
Unsupported languages leave the text unchanged. Preserve selections and
indentation, avoid commenting an extra line when a selection ends at column
zero, and make the operation undoable in one step. Verify mixed-language modes
and overlapping or multiple selections before enabling them.

### TABS-01 / TABS-02 / TABS-03: tabs

Provide configurable compact sizing, with elided titles and full-name/path
tooltips. Keep close buttons and dirty indicators usable. Optional multiple
rows expose overflow tabs without horizontal scrolling; resizing, click hit
testing, keyboard traversal, closing, and dragging between rows must preserve
the logical document order. Existing single-row behavior remains available.

Offer a small color palette and a reset-to-default action in the tab context
menu. Maintain readable text and a visible active/dirty state in both themes.
Colors follow a document when reordered or moved between panes, and survive
normal restart and recovery. They must not depend on tab indices or titles.

### TABS-04: unsaved document titles

Offer **Set Tab Title...** in the tab context menu for documents with no file
path. Trim leading/trailing whitespace; cancellation or a blank entry preserves
the previous title. Update the tab, tooltip and active window title. Keep text,
dirty state and undo history unchanged. Titles follow reordered and moved tabs
and survive sessions and automatic backups, even for clean empty documents.
Identical titles and characters unsuitable for filenames must remain safe.
After saving, use the real filename; saving a copy keeps the custom title.
Existing sessions without title metadata continue to use automatic names.

### TABS-05: insertion preview

After the drag threshold, show a thick vertical line between the tabs where
the dragged document will be inserted. Choose before or after based on the
hovered tab's midpoint; include insertion at the start/end of rows. Use one
calculation for preview and drop, adjusting the destination index after source
removal. Preserve order until release, hide the marker after release/cancel,
and leave order unchanged for outside drops or Escape. Support both single-row
and wrapped modes and right-to-left layout. The marker must remain above tabs
and close buttons without intercepting mouse events.

### DIFF-01: comparison

Compare the current in-memory contents of two documents, including unsaved
edits. Show both documents side by side, with removed lines in red on the left
and added lines in green on the right. Treat the left document as the reference
and the right document as the compared version; modified lines appear as red
and green ranges. Unchanged lines retain their normal appearance.

The user explicitly requested highlighting only: no next/previous-change
controls, merge buttons, staging, or other additional diff controls. A basic
command to choose the two documents and enter/leave comparison is necessary.
The user subsequently requested temporary editing locks (2026-10-11). Keep both
compared documents read-only until comparison ends; restore the previous mode,
including existing read-only documents. Do not change document contents,
selection, undo history or dirty state. Clear decorations on leaving comparison.
Provide a toolbar entry as well as View and tab-context menu actions. The toolbar
command must also end an active comparison. On exit, return surviving documents
to their original panes and tab positions, remove an empty comparison-only pane,
and avoid taking documents back from another window. These refinements were
requested and implemented on 2026-10-11.
Handle empty documents, final-newline differences, and LF/CRLF consistently.

## Standard user installation

### INSTALL-01: standard per-user installation

Prepared on 2026-10-11: `install-user.sh` installs as **Notepadqq** under
`~/.local`, with the standard command and desktop identity, without `sudo`.
Import the development profile once, preserve unsaved-document and recovery
cache references, back up any pre-existing standard profile, and retain live
sessions on subsequent updates. Other packages and the development launcher
are removed separately by the user. Preparation and tests must not change
the user's actual installation. See [packaging](../PACKAGING.md) for usage.

## Delivery order

1. Establish a buildable isolated baseline and run the existing tests.
2. Add comment shortcuts and compact/colorable tabs.
3. Add multiple-row tabs.
4. Add side-by-side red/green diff highlighting.

Record errors encountered during any milestone in the bug log. Reopen historical
reports only if symptoms recur; prioritize reproduced regressions by severity.

Each implementation updates this tracker and the README with actual behavior,
limitations, and verification results. Session metadata changes require focused
save/load and backup-path coverage. No recovery-format migration is planned.
