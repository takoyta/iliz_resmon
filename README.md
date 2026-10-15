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

## Building

You need Visual Studio 2022 Build Tools with the C++ workload. Run `build.bat`: it compiles all `src\*.cpp` into `iliz_resmon.exe` in the project root. If Build Tools are installed elsewhere, set the `VSINSTALL` environment variable to their path.

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
| `nocrt.cpp` | CRT-free build: entry point, `memset`/`memcpy`/`memcmp`, crash dialog |

## License

[MIT](LICENSE)
