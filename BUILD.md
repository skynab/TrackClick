# Point-N-Click — Qt6 Build Instructions

A cross-platform virtual mouse / dwell-clicker, inspired by Polital Enterprises'
Point-N-Click, rebuilt in C++17 + Qt6. Uses the TrackIR color scheme
(#FFA600, #2D2D2D, rgba(0,0,0,0.5)).

---

## Prerequisites

| Platform | Requirement |
|---|---|
| All | Qt 6.2+ (`Core`, `Gui`, `Widgets`), CMake 3.16+, C++17 compiler |
| Windows | MSVC 2019+ or MinGW-w64; `user32` is linked automatically |
| macOS | Xcode 13+; `ApplicationServices` framework linked automatically |
| Linux | `libxtst-dev` (Debian/Ubuntu) or `libXtst-devel` (Fedora/RHEL) |

### macOS — Accessibility permission
The app injects mouse events via CGEvent. macOS will prompt you to grant
**Accessibility** permission the first time (System Settings → Privacy & Security
→ Accessibility). Without it clicks won't reach other apps.

### Linux — Wayland note
The Linux backend uses X11/XTest. On Wayland-only sessions (no XWayland),
click injection is not supported. Running with `XDG_SESSION_TYPE=x11` or
enabling XWayland will work.

---

## Build

```bash
git clone <this repo>
cd PointNClick

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/PointNClick          # Linux/macOS
.\build\Release\PointNClick.exe   # Windows
```

### Optional: specify Qt installation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

---

## Features

- **Frameless floating toolbar** — always on top, drag by the title bar
- **Click types**: Left / Right / Middle — single, double, drag start/stop
- **Scroll**: Up, Down, Left, Right
- **Modifier toggles**: Ctrl, Alt, Shift (one-shot, cleared after click)
- **AutoMouse (dwell clicking)** — hover for a configurable time to click automatically
  - Orange progress bar shows dwell countdown
  - Re-arms automatically for continuous use
- **Settings dialog** — dwell time, sensitivity, which buttons are visible, opacity
- **System tray** — hide to tray; double-click tray icon to restore
- **Persistent settings** — window position, all preferences saved via QSettings
- **TrackIR color scheme** — #FFA600 orange on #2D2D2D dark gray

---

## Project Structure

```
PointNClick/
├── CMakeLists.txt      — Build system
├── main.cpp            — Entry point
├── mainwindow.h/cpp    — Floating toolbar UI
├── dwellmanager.h/cpp  — AutoMouse dwell-click engine
├── clickinjector.h/cpp — Platform-specific mouse event injection
└── settingsdialog.h/cpp— Configuration dialog
```
