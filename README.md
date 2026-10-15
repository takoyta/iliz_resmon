# iliz resmon

English | [Русский](README.ru.md)

A tiny resource monitor for the Windows system tray: CPU, memory, disks and network. Pure WinAPI C++ with no CRT and no third-party libraries. The exe is about 35 KB and uses very little memory.

<p>
<img src="docs/cpu.png" height="300"> <img src="docs/mem.png" height="300"> <img src="docs/disk.png" height="300"> <img src="docs/net.png" height="300">
</p>

The UI is in English or Russian, picked from the Windows display language (Russian for a Russian UI, English otherwise).

## Features

- **CPU**: a tray icon with one bar per core (cores are grouped when there are more cores than pixels). The popup shows a 5-minute graph (average load and the busiest core) and the top 10 processes.
- **Memory**: used and available memory, cache, commit charge, a 5-minute graph, and the top 10 processes by private working set.
- **Disks**: one entry per physical drive with its volume letters (`Samsung SSD 980 PRO [C:]`): activity, read and write speed, queue length, free space, and the processes loading each drive.
- **Network**: download and upload speed, a 5-minute graph, and the top 10 processes by traffic with their current speed and today's totals (reset at midnight).
- Processes that drop out of a top list stay visible in grey for 5 seconds, so you have time to see what it was.
- DPI-aware (per-monitor v2), with correct popup placement, including icons in the hidden-icons flyout.
- **Settings**: the tray menu item **Settings…** opens `iliz_resmon_config.exe`, a small dialog where you choose which of the four tray icons to show and whether to start with Windows. Changes apply immediately; at least one icon stays on.
- Autostart (the **Start with Windows** checkbox in the configurator) through `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, with no admin rights needed.

## Administrator rights

Per-process disk and network statistics come from kernel tracing (ETW), which needs administrator rights. Without them everything else still works. The "Restart as administrator" menu item starts an elevated copy, which closes the non-elevated instance by itself.

Some anti-cheat systems (for example, Easy Anti-Cheat) block kernel tracing while a game is running. The program retries every 15 seconds and recovers on its own after you exit the game.

## Settings

Settings live in `iliz_resmon.ini` next to the exe:

```ini
[tray]
cpu=1
mem=1
disk=1
net=1
```

A missing file or key means "on". `iliz_resmon_config.exe` writes the file and posts the registered window message `iliz_resmon_reload` to the running iliz resmon, which rereads the file and re-adds only the enabled icons (the message is allowed through UIPI, so it works when iliz resmon runs as administrator). The ini is not part of the release zip and is listed in `preserve` in `iliz_updater.json`, so updates keep your settings.

## Updates

The release zip contains `iliz_updater.exe` and `iliz_updater.json` from
[iliz_updater](https://github.com/takoyta/iliz_updater). Keep them next to `iliz_resmon.exe`.

- On start and then every 6 hours the program runs a quiet check against the GitHub releases of this repository. The updater itself limits real requests to once per 24 hours (`check_interval_hours`).
- The tray menu has **Check for updates**. When a newer release is found, the item becomes **Update to vX.Y.Z**.
- The item opens the updater window with the current and new version and the release notes. After you confirm, it downloads the zip, checks SHA-256, closes iliz resmon, replaces the files (rolling back on any error) and starts it again with the same rights.
- Without `iliz_updater.exe` next to the exe (for example, a local build) the menu item is hidden.

Publishing releases is described in [RELEASING.md](RELEASING.md).

## Building

You need Visual Studio 2022 Build Tools with the C++ workload. Run `build.bat`: it compiles all `src\*.cpp` into `bin\iliz_resmon.exe` and `config\config.cpp` (plus `src\nocrt.cpp`) into `bin\iliz_resmon_config.exe` and puts `iliz_updater.json` (and `iliz_updater.exe` from `..\iliz_updater\bin`, if present) next to it, so `bin\` is a ready-to-run folder. If Build Tools are installed elsewhere, set the `VSINSTALL` environment variable to their path.

## Code layout (`src/`)

| File | Purpose |
|---|---|
| `main.cpp` | Entry point, window, menu, single instance, handoff to the elevated instance |
| `app.cpp` | Once-a-second loop: sample, process, render |
| `cpu.cpp`, `mem.cpp`, `disk.cpp`, `net.cpp` | Metric sampling |
| `procs.cpp` | Process snapshot (`NtQuerySystemInformation`): top lists by CPU, memory and I/O |
| `etw.cpp` | Kernel tracing: per-process disk and network activity, automatic session recovery |
| `history.cpp` | 5-minute history for the graphs |
| `sticky.cpp` | Rows that dropped out of a top list and stay visible in grey |
| `trayicon.cpp` | Icon drawing and tray handling |
| `popup.cpp`, `views.cpp`, `draw.cpp`, `theme.h` | Popups: placement, content, drawing primitives, colors |
| `autostart.cpp` | Elevation check, restart as administrator |
| `settings.h` | `iliz_resmon.ini` read/write, the reload message and autostart (HKCU Run), shared with the configurator |
| `../config/` | `iliz_resmon_config.exe`: settings dialog (`config.cpp`, `config.rc`, manifest) |
| `lang.h`, `lang.cpp` | English and Russian UI strings, shared with the configurator |
| `format.cpp` | Speed and size formatting |
| `version.h`, `app.rc` | Version (one place) and the VERSIONINFO resource |
| `iliz_updater_client.h` | Copy of the CRT-free iliz_updater client: background check, launching the update |
| `nocrt.cpp` | CRT-free build: entry point, `memset`/`memcpy`/`memcmp`, crash dialog |

## License

[MIT](LICENSE)
