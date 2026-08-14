# Config editor foundation

This document fixes the storage contract used by the experimental in-game
config editor.  The editor is data-driven, but the user's Quake CFG files stay
the source of truth.

## Managed files

The machine-readable list is stored in `qw/config_editor/managed_files.json`.
Only the following files are managed in the first implementation:

- `fortress/settings.cfg` - common settings and commands;
- `fortress/binds.cfg` - common binds;
- `fortress/hud.cfg` - raw HUD configuration;
- `fortress/scout.cfg`;
- `fortress/sniper.cfg`;
- `fortress/soldier.cfg`;
- `fortress/demoman.cfg`;
- `fortress/medic.cfg`;
- `fortress/hwguy.cfg`;
- `fortress/pyro.cfg`;
- `fortress/spy.cfg`;
- `fortress/engineer.cfg`.

`qw/config.cfg`, `qw/autoexec.cfg` and the stock files below
`ezquake/ezquake/cfg` are deliberately outside the editable set.

## Inheritance

Every class CFG executes `settings.cfg`.  The common settings file in turn
executes `binds.cfg` and `hud.cfg`.  A class context therefore has the
following effective order:

1. common binds;
2. HUD;
3. common settings and commands;
4. the selected class CFG.

The last matching statement wins.  A value found only in a parent file is
reported as inherited; a class-local statement is reported as an override.
The editor never copies inherited statements into a class file merely because
the file was opened.

## Lossless editing rules

- Files are treated as byte sequences.  Their encoding and line endings are
  not normalised.
- Every parsed node owns an exact byte range and records its source file,
  stable node id, and starting/ending line.
- Serialising an unchanged document must reproduce the original bytes exactly.
- Editing one node replaces only that node's byte range.  Other nodes remain in
  their original order and retain their original whitespace and comments.
- `Misc` is a menu projection, not a storage section.  Each displayed row is
  identified by `(file_id, node_id, line_start, line_end)` and is written back
  to that original node.
- Unknown or malformed statements are preserved and are never executed by the
  parser.

## Dictionary files

- `qw/config_editor/dict_settings.json` describes settings and their widgets;
- `qw/config_editor/dict_binds.json` describes bindable actions;
- their contracts are defined by the adjacent JSON Schema files.

The schema describes semantics independently from storage.  The current
backend is Quake CFG; a future JSON backend can use the same widgets by adding
a new `storage.type` adapter.

## Foundation controls

- `checkbox` stores the dictionary's exact `checked_value` or
  `unchecked_value`; it is not restricted to `0` and `1`.
- `key_capture` records up to four staged key values without calling
  `Key_SetBinding`. Modifier-only presses are ignored, while `Ctrl+key`,
  `Alt+key`, and `Shift+key` use the client's native combination key codes.
- `textarea` owns a dynamically sized byte buffer, supports multiline editing
  and scrolling, and can render an optional source gutter. Each gutter row
  carries the original file, source line, and node id so the menu can project
  several files without changing their on-disk order.

The controls alter only the file-backed draft. Applying draft changes to live
cvars or writing dirty files is a separate, explicit operation that will be
implemented with the menu pages.
