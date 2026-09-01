# ezquake-tf updater

The Windows client checks the latest release published in
`ezh-HaMMeR/ezquake-tf` without delaying startup. The default behavior is
notification-only: an update is never installed automatically during a match.

## Player workflow

1. Select `Update client` in the main menu or run `/update`.
2. If a newer version is available, repeat the action to download it.
3. After the SHA-256 check succeeds, repeat the action once more to install it.
4. The client exits cleanly, `update.exe` waits for every `ezquake.exe` using
   the same installation path to close, replaces the managed files, and starts
   the client again with the original command-line parameters.

The once-per-launch background check is always enabled. It only reads release
metadata; downloading and installing still require an explicit player action.
`/update_status` prints the current state.

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
