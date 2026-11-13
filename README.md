# Sift

A light SQLite client for Omarchy: write queries with completion, browse the
schema, and read results without leaving the keyboard. It follows the Omarchy
theme and language live.

![Sift showing a products table with the row inspector open](docs/screenshot.png)

- **Editor** with SQL highlighting, line numbers, auto-indent, and completion
  as you type: tables after `FROM`/`JOIN`, columns of the tables in the
  statement, columns after an alias (`o.`), keywords, and functions.
- **Ctrl+Enter** runs the statement under the cursor (or the selection);
  queries run in the background and **Esc** cancels them. Errors put the
  cursor where SQLite found the problem.
- **Schema browser** with row counts, primary and foreign keys, indexes, and
  a filter over tables and columns. Click a table to see its rows, click a
  column to insert its name.
- **Results grid** with typed headers, NULLs and numbers set apart, click to
  sort, arrows to move, Ctrl+C to copy a cell.
- **Row inspector** shows the selected row field by field, JSON indented and
  blobs as hex.
- History, CSV copy and export, a scratch database in memory, and a draft per
  database that survives restarts.
- DROP, and DELETE or UPDATE without WHERE, ask before running.

## Shortcuts

| Key | Does |
| --- | --- |
| Ctrl+Enter | Run the statement under the cursor, or the selection |
| Ctrl+Shift+Enter | Run everything in the editor |
| Esc | Cancel a running query, close lists |
| Ctrl+Space | Suggest tables, columns, keywords |
| Tab | Insert a suggestion (Enter too, once picked with the arrows) |
| Ctrl+/ | Comment or uncomment lines |
| Tab / Shift+Tab | Indent or outdent lines |
| Ctrl+Z / Ctrl+Shift+Z | Undo, redo |
| Ctrl+H | Query history |
| Ctrl+K | Filter tables and columns |
| Ctrl+1 / 2 / 3 | Focus tables, editor, results |
| Arrows, Ctrl+C | Move through results, copy a cell |
| Ctrl+Shift+C | Copy results as CSV |
| Ctrl+E | Export results as CSV |
| Ctrl+R | Reload the schema |
| Ctrl+O | Open a database |
| Ctrl+W | Close the database |
| F1 / ? | Show or hide the shortcuts |
| Ctrl+Q | Quit |

Typing goes to the editor, so the single-key shortcuts other Omarchy apps use
(`Q`, Space) are on Ctrl here.

## Build

```sh
bin/build                 # build/sift
build/sift some.db        # open a database (a new path creates one)
bin/test                  # run the tests offscreen
bin/install               # package and install, so it shows in the launcher
```

Needs `qt6-base`, `qt6-declarative`, `qt6-wayland`, and `sqlite`.
History, recent files, and drafts live in `~/.local/state/sift/`.
