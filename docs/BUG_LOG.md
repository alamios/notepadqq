# Bug log

Maintainer: **alamios**. Updated: 2026-10-10.

Record unexpected behavior encountered during feature development and daily use.
Distinguish user observations, assumptions, and reproduced defects. An upstream
warning alone is not a confirmed bug in this checkout.

## Current status

No active defect has been reproduced in the fork. This is not a stability
guarantee: the fork has not yet been compiled or runtime-tested by the agent.
New features are the current implementation priority.

| ID | Affected build | Symptom | Status and evidence | Next action |
| --- | --- | --- | --- | --- |
| BUG-01 | Flatpak 1.4 | Missing scrollbar and non-working mouse wheel | User reports scrolling works in 2.1 | Monitor; reopen only if it recurs |
| BUG-02 | apt `2.0.0~beta1-4.1build1` | Applying any preference hangs with high CPU use | User reports no hangs in 2.1; earlier high CPU was observed in the apt process | Monitor; reopen only if it recurs |
| BUG-03 | Older installed build, reported in the Flatpak 1.4 context | After a crash, current sessions and older copies reopen together; files are not lost | Assumed fixed in 2.1 at the user's request; recovery has not been verified | Deferred; record recurrence if observed |

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
