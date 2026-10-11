# Bug log

Maintainer: **alamios**. Updated: 2026-10-11.

Record unexpected behavior encountered during feature development and daily use.
Distinguish user observations, assumptions, and reproduced defects. An upstream
warning alone is not a confirmed bug in this checkout.

## Current status

The development fork compiles with Qt 6.10.2 and passes both CTest targets.
Comment toggling, compact tabs, tab colors, unsaved titles and drag insertion
previews and read-only comparison pass automated Qt/WebEngine validation using
temporary settings: 88 UI test results and 28 C++ correctness
test results, with no failures. The user confirms that comment toggling and the
new tab features work in the installed fork. The user accepts the latest tab
dragging behavior on Plasma. Telemetry removal is verified
in source and the built binary; legacy-key cleanup has regression coverage.
Insertion previews have twelve passing regression cases and user acceptance.
Comparison has automated picker, highlighting, editing-lock and lifecycle
coverage. The user confirms the initial comparison works on Plasma; the latest
toolbar toggle and original-pane restoration still need desktop confirmation.
The requested comparison is implemented without navigation or merge controls.

| ID | Affected build | Symptom | Status and evidence | Next action |
| --- | --- | --- | --- | --- |
| BUG-01 | Flatpak 1.4 | Missing scrollbar and non-working mouse wheel | User reports scrolling works in 2.1 | Monitor; reopen only if it recurs |
| BUG-02 | apt `2.0.0~beta1-4.1build1` | Applying any preference hangs with high CPU use | User reports no hangs in 2.1; earlier high CPU was observed in the apt process | Monitor; reopen only if it recurs |
| BUG-03 | Older installed build, reported in the Flatpak 1.4 context | After a crash, current sessions and older copies reopen together; files are not lost | Assumed fixed in 2.1 at the user's request; recovery has not been verified | Deferred; record recurrence if observed |
| BUG-04 | EDIT-01 implementation, fixed in `9b4859b` | A repeated toggle did not remove inserted comments after selection movement; an intermediate selection-array fix also caused JavaScript errors | Fixed with selection bookmarks and copied range data; automated tests pass and the user confirms the installed feature works | Closed; not an upstream defect |
| BUG-05 | Initial TABS-03 implementation | Changing only a tab color did not update its automatic backup | Reproduced by creating a backup, changing color without editing text and waiting for the next backup; change detection now includes color metadata; regression test passes for assigning and removing a color | Closed in the compact/color tabs milestone; not an upstream defect |
| BUG-06 | Compact tabs through `295409b` | Tabs stay too narrow even with available space; the width setting appears ineffective | User report on Plasma; earlier tests checked only upper bounds. Size hints now measure the complete label, icons and buttons, and minimum hints prevent unwanted shrinking. Tests verify exact capped widths and enough room for short names | Fixed in this implementation; native Plasma confirmation pending |
| BUG-07 | Colored tabs through `295409b` | Color band touches the save icon | User report; tab sizing now reserves a band area and native labels are painted below it | Fixed in this implementation; native Plasma confirmation pending |
| BUG-08 | Fork options through `295409b` | New actions and options lack translations | Missing catalog entries confirmed; all new strings now have translations in the 14 supported locales, with compiled-catalog tests | Closed in this implementation |

## Upstream crash warning

The upstream README reports possible unexpected crashes with recent OS/Qt
versions. No specific crash from this source baseline has been reported or
reproduced here. Retain that context while tracking concrete incidents instead
of scheduling a speculative stability rewrite.

## Recording a new or recurring error

Use a new stable `BUG-XX` identifier, or reopen an existing entry when the same
symptom recurs. Include:

- Summary, status (`reported`, `reproduced`, `in progress`, `fixed`, or
  `monitoring`), severity, and observation date.
- Commit/tag, package format, OS/desktop/session type, Qt version, and editor
  backend. Distinguish a packaged release from a fork build.
- Reproduction steps, expected behavior, actual behavior, and frequency.
- Relevant sanitized logs or stack traces and whether unsaved data is affected.
- Workaround, suspected cause (clearly labeled), and links to related work.
- Fix commit and validation results when available, including any remaining
  uncertainty. Keep the historical report when closing it.

Reproduce with an isolated profile and disposable/copied documents. Do not
alter real recovery data to diagnose a problem. Feature-related tests should
guard the behavior they change; investigate broader failures when evidence
justifies it.
