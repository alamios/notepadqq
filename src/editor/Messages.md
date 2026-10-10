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
