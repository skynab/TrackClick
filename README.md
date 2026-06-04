# TrackClick

A cross-platform virtual mouse / dwell-clicker for Windows, macOS, and Linux, built in C++17 + Qt6.

## Features

- **Floating toolbar** — frameless, always-on-top, draggable
- **Click types** — Left, Right, Middle; single, double, drag start/stop
- **Scroll** — Up, Down, Left, Right
- **Modifier toggles** — Ctrl, Alt, Shift (one-shot, cleared after click)
- **AutoMouse (dwell click)** — hover to click automatically after a configurable delay; orange progress bar shows countdown
- **Settings dialog** — dwell time, sensitivity, visible buttons, opacity, button layout, icon-only mode
- **Button layouts** — Rectangle (grid), Horizontal (one row), or Vertical (one column)
- **Multilingual UI** — English, Čeština, Français, Español, 中文简体, 日本語, 한국어; language switches live without restart
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

### Compiling translations to .qm (optional optimisation)

All translation `.ts` files are embedded directly in the binary via `resources.qrc` and parsed at runtime — **no external tools are required** for any language to work.

If Qt LinguistTools (`lrelease`) happen to be installed, CMake automatically compiles the `.ts` files to binary `.qm` format, which loads slightly faster. You can also do this manually:

```bash
lrelease translations/trackclick_cs.ts    -qm build/translations/trackclick_cs.qm
lrelease translations/trackclick_fr.ts    -qm build/translations/trackclick_fr.qm
lrelease translations/trackclick_es.ts    -qm build/translations/trackclick_es.qm
lrelease translations/trackclick_zh_CN.ts -qm build/translations/trackclick_zh_CN.qm
lrelease translations/trackclick_ja.ts    -qm build/translations/trackclick_ja.qm
lrelease translations/trackclick_ko.ts    -qm build/translations/trackclick_ko.qm
```

Place the resulting `.qm` files in a `translations/` folder next to the executable. The app searches that directory first and falls back to the embedded `.ts` copy.

### Updating translations without rebuilding

Translation files are searched in this order at startup and on every language switch:

| Priority | Location | Notes |
|---|---|---|
| 1 | `<user-data>/translations/` | Drop a file here to override the bundled copy |
| 2 | `<binary>/translations/` | Shipped alongside the executable |
| 3 | `<binary>/../translations/` | macOS app bundle |
| 4 | Embedded in binary | Built-in fallback, always present |

**User-data paths by platform:**

| Platform | Path |
|---|---|
| macOS | `~/Library/Application Support/TrackClick/translations/` |
| Windows | `%LOCALAPPDATA%\TrackClick\TrackClick\translations\` |
| Linux | `~/.local/share/TrackClick/translations/` |

Place a file named `trackclick_<code>.ts` (e.g. `trackclick_fr.ts`) in that directory. The app picks it up the next time that language is selected — no restart or rebuild needed. A compiled `.qm` file of the same stem is accepted too and loads slightly faster.

### Adding a new language

1. Create `translations/trackclick_<code>.ts` following the format of an existing file.
2. Add it to `resources.qrc` (under the `/translations` prefix) so it is embedded in the binary.
3. Add a `m_cmbLanguage->addItem(...)` entry in `SettingsDialog::buildUi()` (`settingsdialog.cpp`).
4. *(Optional)* Add it to the `TS_FILES` list in `CMakeLists.txt` so CMake compiles a `.qm` file when LinguistTools are available.

## Project Structure

```
TrackClick/
├── CMakeLists.txt        — Build system
├── resources.qrc         — Embedded icons and translation files
├── icons/                — SVG button icons
├── translations/         — Qt Linguist .ts source files (cs, fr, es, zh_CN, ja, ko)
├── main.cpp              — Entry point
├── mainwindow.h/cpp      — Floating toolbar UI
├── dwellmanager.h/cpp    — AutoMouse dwell-click engine
├── clickinjector.h/cpp   — Platform-specific mouse event injection
└── settingsdialog.h/cpp  — Configuration dialog & AppSettings struct
```

## License

See [LICENSE](LICENSE).
