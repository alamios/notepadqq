# Fork requirements

Recorded: 2026-10-10. Status: requirements and initial design; no runtime
changes have been implemented yet.

## Purpose and baseline

Improve the existing Notepadqq editor for daily use on Kubuntu 26.04. Preserve
its language support, document editing, configurable shortcuts, tabs, and
restoration of saved and unsaved documents. Retain Qt and the current editor
engine instead of porting Notepad++ or replacing the engine.

The immutable starting reference is `upstream-baseline` at
`37652c5efa5fe8720f4eaf9f2e459888c1c6d6d0` (development version 2.2.0).
Development starts on `fork/development`, with the original repository recorded
as the `upstream` remote. A hosted fork is not yet configured.

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
| EDIT-01 | P1 | `Ctrl+Shift+C` comments/uncomments lines and selected code blocks using the active language | Designed |
| TABS-01 | P1 | Smaller tabs | Designed |
| TABS-02 | P2 | Tabs wrap into multiple rows | Designed; largest tab-layout change |
| TABS-03 | P1 | User-selectable tab colors, preserved across restart | Designed |
| DIFF-01 | P2 | Compare two documents in parallel with Git-style red/green differences | Designed |

Additional requests can be added with stable IDs when the user recalls them.

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

### DIFF-01: comparison

Compare the current in-memory contents of two documents, including unsaved
edits. Show both documents side by side, with removed lines in red on the left
and added lines in green on the right. Treat the left document as the reference
and the right document as the compared version; modified lines appear as red
and green ranges. Unchanged lines retain their normal appearance.

The user explicitly requested highlighting only: no next/previous-change
controls, merge buttons, staging, or other additional diff controls. A basic
command to choose the two documents and enter/leave comparison is necessary.
Recompute after edits without blocking the UI or changing document contents,
selection, undo history, or dirty state. Clear decorations on leaving comparison.
Handle empty documents, final-newline differences, and LF/CRLF consistently.

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
