# Feature guide

## Comment shortcuts

Choose **Edit > Toggle Comment** or press `Ctrl+Shift+C`. An empty selection
comments or uncomments the current line; selections covering complete lines
use line comments. Partial selections use block comments when the language
supports them, otherwise line comments. Each toggle is one undo step.
Unsupported languages and read-only documents are left unchanged.

Bindings are configurable in **Settings > Preferences > Shortcuts**.

## Tabs

Open **Settings > Preferences > Appearance** to configure:

- **Compact tabs**: reduce tab padding.
- **Always show full tab titles**: use the width required by each title;
  enabled by default.
- **Maximum compact tab width**: cap compact titles when full titles are
  disabled; default 160 logical pixels, range 96–320. Long names use middle
  elision, with their full name/path in the tooltip. The control is directly
  below the full-title option.
- **Wrap tabs into multiple rows**: move overflow tabs onto successive rows;
  enabled by default. Disabling it restores a single row with scroll buttons.
  A title wider than one pane can scroll within its own row.

Drag a tab to reorder it. A thick line shows the insertion boundary, including
between rows. Releasing commits the move; Escape or releasing outside the tab
strip cancels it.

Right-click a tab and select **Tab Color** to assign red, orange, yellow, green,
blue or purple, or **Default** to clear the color. The color band does not
change the document's modified state.

For an unsaved document, choose **Set Tab Title...** from its tab context menu.
Blank input and Cancel leave the title unchanged. Saving the document replaces
its custom title with the filename; saving a copy retains the unsaved title.
Colors and custom titles follow moved tabs and persist in sessions and
automatic backups, including empty titled documents.

## Document comparison

1. Select the reference document.
2. Choose **View > Compare with...**, the tab context-menu command, or the
   red/green comparison button in the toolbar.
3. Choose another open document. Paths and view/tab positions distinguish
   documents with identical titles.

The reference appears on the left with removed lines in red. The other
document appears on the right with added lines in green. Modified lines are
yellow on both sides. Within each unmatched block, lines are paired in document
order as replacements; surplus lines remain red or green. This is a line-level
heuristic, without character-level matching.
Comparison uses current contents, including unsaved edits. Syntax highlighting
remains visible, and both documents are temporarily read-only.

Click the toolbar button again or choose **View > End Comparison** to finish.
The documents return to their previous panes and tab positions, and their
previous editing modes are restored. A pane created only for comparison closes
when empty. Closing a compared document, moving it outside the window, or
putting both documents in one pane also ends comparison. Documents moved to
another window stay there.

One comparison is active per window. Highlights cover complete lines, with
independent scrolling and no merge, navigation or character-level controls.
LF and CRLF compare equally; a final-newline difference marks the extra empty
line. Identical documents produce a status message.

Comparison runs in the background. Limits are 4 Mi UTF-16 code units and
100,000 lines combined, 20,000 lines in the changed region, and 4,000,000 LCS
table cells. Exceeding a limit ends comparison with a status message and restores
editing. Equal leading/trailing lines are trimmed before calculating differences.

The shortcut has no default binding and can be set in Preferences. Existing
custom toolbars receive the comparison action once; later customization is
respected. Comment and comparison actions are currently CodeMirror-only.

## Menu bar

Press `Ctrl+M` to toggle the menu bar. This is the default shortcut for
**View > Show Menubar** and can be changed in Preferences.
