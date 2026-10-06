# Isolated data directory

Set `BSTEP_DATA_DIR` to an absolute directory before starting the host process to
keep the lab's B-Step projects, snapshots, presets, mappings and logs separate
from the normal installation. All B-Step instances in that host use this root.

For example on macOS:

```sh
BSTEP_DATA_DIR=/absolute/path/to/lab/bstep-data /path/to/REAPER.app/Contents/MacOS/REAPER
```

The normal platform directory remains the default when the variable is unset,
empty or relative. Setting it in a terminal after REAPER starts has no effect on
the running host. To restore the normal location, quit the host and restart it
without the variable. Existing files are not copied or moved automatically.

The lab's macOS build already uses this change alongside the host-tempo branch;
the directory override itself is independent of that clock change. Linux
production does not need this variable. Plugin identifiers, MIDI behavior and
saved-state formats are unchanged.
