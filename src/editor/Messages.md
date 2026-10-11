=== MESSAGES ===

C++ --> JS
----------

* C_CMD_TOGGLE_COMMENT()
  Toggle comments using the active language. Empty/full-line selections use
  line comments (or full-line block comments if the language has no line syntax).
  Partial selections use block syntax where available, otherwise line comments.
  Overlapping line targets are merged; all changes form one undo step.
  Unsupported languages and read-only documents are left unchanged.
  Implemented by the CodeMirror backend; Monaco support is not included yet.

JS --> C++
----------

* J_EVT_READY()
  Notify when the editor is fully loaded.
* J_EVT_CONTENT_CHANGED()

## Document comparison (CodeMirror)

- `C_FUN_DIFF_SNAPSHOT`: asynchronous result `{text, revision}`; text uses LF.
- `C_CMD_DIFF_LOCK`: retain the previous read-only option and lock editing.
- `C_CMD_DIFF_MARKS`: `{revision, side: "removed"|"added", lines: [zeroBasedLine]}`;
  ignore stale revisions and apply background classes without text changes.
- `C_CMD_DIFF_CLEAR`: remove comparison-owned marks, retaining the editing lock.
- `C_CMD_DIFF_END`: clear marks and restore the previous read-only option.

Revisions advance synchronously on content changes. These commands do not mark
documents dirty or modify undo history. Marks and editing locks are temporary
presentation state, not session metadata.
