# TrackClick

A cross-platform virtual mouse / dwell-clicker for Windows, macOS, and Linux, built in C++17 + Qt6.

## Features

- **Floating toolbar** — frameless, always-on-top, draggable
- **Click types** — Left, Right, Middle; single, double, drag start/stop
- **Scroll** — Up, Down, Left, Right
- **Modifier toggles** — Ctrl, Alt, Shift (one-shot, cleared after click)
- **AutoMouse (dwell click)** — hover to click automatically after a configurable delay; orange progress bar shows countdown
- **Settings dialog** — dwell time, sensitivity, visible buttons, opacity
- **System tray** — minimize to tray; double-click to restore
- **Persistent settings** — window position and all preferences saved across sessions

## Requirements

| Platform | Requirement |
|---|---|
| All | Qt 6.2+, CMake 3.16+, C++17 compiler |
| Windows | MSVC 2019+ or MinGW-w64 |
| macOS | Xcode 13+; grant **Accessibility** permission on first launch |
| Linux | `libxtst-dev` (Debian/Ubuntu) or `libXtst-devel` (Fedora/RHEL); X11/XWayland required |

## Build

```bash
git clone <this repo>
cd TrackClick

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/TrackClick              # Linux/macOS
.\build\Release\TrackClick.exe  # Windows
```

To specify a custom Qt installation:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

See [BUILD.md](BUILD.md) for platform-specific notes.

## Project Structure

```
TrackClick/
├── CMakeLists.txt        — Build system
├── main.cpp              — Entry point
├── mainwindow.h/cpp      — Floating toolbar UI
├── dwellmanager.h/cpp    — AutoMouse dwell-click engine
├── clickinjector.h/cpp   — Platform-specific mouse event injection
└── settingsdialog.h/cpp  — Configuration dialog
```

## License

See [LICENSE](LICENSE).
