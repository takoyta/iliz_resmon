# iliz resmon

English | [Русский](README.ru.md)

A tiny resource monitor for the Windows system tray: CPU, memory, disks and network. Pure WinAPI C++ with no CRT and no third-party libraries. The exe is about 35 KB and uses very little memory.

<p>
<img src="docs/cpu.png" height="300"> <img src="docs/mem.png" height="300"> <img src="docs/disk.png" height="300"> <img src="docs/net.png" height="300">
</p>

The UI is currently in Russian.

## Features

- **CPU**: a tray icon with one bar per core (cores are grouped when there are more cores than pixels). The popup shows a 5-minute graph (average load and the busiest core) and the top 10 processes.
- **Memory**: used and available memory, cache, commit charge, a 5-minute graph, and the top 10 processes by private working set.
- **Disks**: one entry per physical drive with its volume letters (`Samsung SSD 980 PRO [C:]`): activity, read and write speed, queue length, free space, and the processes loading each drive.
- **Network**: download and upload speed, a 5-minute graph, and the top 10 processes by traffic with their current speed and today's totals (reset at midnight).
- Processes that drop out of a top list stay visible in grey for 5 seconds, so you have time to see what it was.
- DPI-aware (per-monitor v2), with correct popup placement, including icons in the hidden-icons flyout.
- Autostart through `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, with no admin rights needed.

## Administrator rights

Per-process disk and network statistics come from kernel tracing (ETW), which needs administrator rights. Without them everything else still works. The "Restart as administrator" menu item starts an elevated copy, which closes the non-elevated instance by itself.

Some anti-cheat systems (for example, Easy Anti-Cheat) block kernel tracing while a game is running. The program retries every 15 seconds and recovers on its own after you exit the game.

## Updates

The release zip contains `iliz_updater.exe` and `iliz_updater.json` from
[iliz_updater](https://github.com/takoyta/iliz_updater). Keep them next to `iliz_resmon.exe`.

- On start and then every 6 hours the program runs a quiet check against the GitHub releases of this repository. The updater itself limits real requests to once per 24 hours (`check_interval_hours`).
- The tray menu has **Проверить обновления** (Check for updates). When a newer release is found, the item becomes **Обновить до vX.Y.Z** (Update to vX.Y.Z).
- The item opens the updater window with the current and new version and the release notes. After you confirm, it downloads the zip, checks SHA-256, closes iliz resmon, replaces the files (rolling back on any error) and starts it again with the same rights.
- Without `iliz_updater.exe` next to the exe (for example, a local build) the menu item is hidden.

To publish a release, run `release.bat`. It builds the exe and writes `dist\iliz_resmon-<version>.zip` (exe, updater, its config, license) and `dist\SHA256SUMS`. Upload both to a GitHub release tagged `v<version>`, for example `gh release create v0.16.0 dist\iliz_resmon-0.16.0.zip dist\SHA256SUMS`. The updater exe is taken from `..\iliz_updater\bin` (override with `ILIZ_UPDATER_EXE`). The version lives in `src\version.h` and goes into both the code and the exe's VERSIONINFO, which the updater compares with the release tag.

## Building

You need Visual Studio 2022 Build Tools with the C++ workload. Run `build.bat`: it compiles all `src\*.cpp` into `bin\iliz_resmon.exe` and puts `iliz_updater.json` (and `iliz_updater.exe` from `..\iliz_updater\bin`, if present) next to it, so `bin\` is a ready-to-run folder. If Build Tools are installed elsewhere, set the `VSINSTALL` environment variable to their path.

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
| `autostart.cpp` | Elevation check, restart as administrator, autostart |
| `format.cpp` | Speed and size formatting |
| `version.h`, `app.rc` | Version (one place) and the VERSIONINFO resource |
| `iliz_updater_client.h` | Copy of the CRT-free iliz_updater client: background check, launching the update |
| `nocrt.cpp` | CRT-free build: entry point, `memset`/`memcpy`/`memcmp`, crash dialog |

## License

[MIT](LICENSE)
