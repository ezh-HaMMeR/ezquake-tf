# ezquake-tf updater

The Windows client checks the latest release published in
`ezh-HaMMeR/ezquake-tf` without delaying startup. By default, a newer release
is downloaded, verified, and installed automatically. Set `autoupdate 0` in a
CFG to keep the version check but disable automatic installation.

## Player workflow

1. The client checks for a newer release during startup.
2. With `autoupdate 1` (the default), it downloads and verifies the archive in
   the background, then exits cleanly to begin installation.
3. `update.exe` waits for every `ezquake.exe` using
   the same installation path to close, replaces the managed files, and starts
   the client again with the original command-line parameters.

The once-per-launch background check is always enabled. With `autoupdate 0`, a
new release is only reported in the console and can still be installed manually
with `/update`. `/update_status` prints the current state.

## Safety model

- Release metadata and the archive are downloaded over HTTPS from GitHub.
- The archive must match the SHA-256 digest published by GitHub Releases.
- ZIP paths are validated before extraction; absolute paths and `..` are
  rejected.
- Only `ezquake.exe`, `update.exe`, and the shipped JSON/PNG resources below
  `qw/` may be replaced.
- `fortress/`, CFG files, demos, screenshots, logs, and all unknown files are
  outside the managed set.
- Existing managed files are moved to `.update/backup/<version>/` before the
  replacement. If any replacement fails, the updater rolls back files already
  changed in that transaction.
- `.update/update.log` records the last installation attempt.

The first release containing `update.exe` still has to be installed manually.
All later Windows releases can update that installation in place while keeping
the `ezquake.exe` filename, location, shortcut, and launch parameters.
