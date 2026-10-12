# FencesT

Native C++ desktop organizer for **Windows 11 x64**. Create groups of application shortcuts, folders and files, then give every group its own material, transparency and rounded corners.

**Status: 0.1.1, working preview.** This is an independent implementation, not affiliated with Stardock or Apple.

## Features

- Three materials: **Liquid Glass**, **Transparent**, **Dark Matte**.
- Per-group background transparency, tint, image and image opacity. Icons/text keep full opacity.
- Adjustable corner radius **0–64 px** and icon size **24–72 px**.
- Optional application/file labels: icons-only mode.
- Drag files from Explorer onto a group to create references. Double-click to open them.
- Move, resize, collapse, duplicate and lock groups.
- Live folder groups; local folder changes are detected automatically.
- Search the selected group's shortcuts by name or path.
- Grid snapping (hold **Shift** to bypass).
- Export/import layouts, atomic saves, backup/recovery and single-instance protection.
- System tray, optional startup at login and configurable global shortcuts (Ctrl/Alt/Shift/Win + key), conflict detection, disable and reset.
- Per-monitor DPI handling and recovery when monitors disconnect.

## Run

Download the ZIP from the latest successful **[Windows build](https://github.com/orcspace/FencesT/actions/workflows/windows.yml)** → **Artifacts** → `FencesT-windows-x64`. Extract the inner package and run **FencesT.exe**. No administrator rights or extra runtime installer are required.

The first start creates two sample groups and opens the Russian settings interface. Right-click a group, use its `…` button or double-click the tray icon to open settings. Closing settings leaves groups running; use **Exit** in the tray menu to quit.

Full guide: [Русская инструкция](docs/QUICKSTART.ru.md).

## Controls

| Action | Control |
| --- | --- |
| Show/hide groups | Win + Alt + F |
| New group | Win + Alt + N |
| Move | Drag the group header |
| Resize | Drag bottom-right corner |
| Collapse/expand | Chevron or double-click header |
| Open shortcut | Double-click, or arrows + Enter |
| Group settings | F2 or `…` |
| Refresh contents | F5 |
| Ignore snap grid | Hold Shift while finishing a drag |

The two global shortcuts can be changed in **Настройки → Горячие клавиши**. Choose modifiers and a primary key, then click **Применить клавиши**. Select **Отключено** to disable an action, or **Вернуть стандартные** to restore the defaults. Conflicting/duplicate combinations are rejected and the previous bindings restored. F12 is reserved by Windows and excluded. Local F2/F5/navigation keys remain fixed. Version 0.1 layouts load with the original defaults; newer exports include the chosen hotkeys.

## How it works / current limits

Groups are independent native desktop tool windows. They contain **references** to files: adding/removing a shortcut never moves or deletes the original. Existing Windows desktop icons remain in place. This preview does not patch Explorer, inject into it, replace the Windows shell or automatically rearrange its icon grid.

**Liquid Glass is a glass-inspired visual style:** a cached, blurred desktop wallpaper, tint, highlights and a translucent edge. It is not Apple's renderer and does not refract live video or windows behind a group. Transparent mode uses real per-pixel alpha. Blur uses the wallpaper rather than screen capture. Wallpaper changes are picked up through Windows setting notifications; third-party animated wallpapers are not sampled.

The current limit is 48 groups and 512 entries per group. Folder groups show direct, non-hidden children, sorted by name, and are read-only. Manual shortcuts are retained when a folder connection is enabled and restored when it is disabled. Network folders or third-party shell extensions can be slow while Windows retrieves their icons. Layout exports contain paths, not copies of target files or images. Keep custom background images at their original paths.

Windows shell behavior (Show Desktop, multiple virtual desktops, Explorer restarts) still needs manual testing on Windows 11. Do not treat the preview as a production-complete replacement for a mature shell utility. See [test checklist](docs/TESTING.md).

## Build

### Visual Studio 2022 / Windows SDK

Install **Desktop development with C++**, CMake and a Windows 11 SDK, then:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cpack --config build/CPackConfig.cmake -C Release -B dist
```

### Cross-compile with MinGW-w64

```sh
cmake -S . -B build-win -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-win --parallel
```

Core serialization tests also run natively on Linux/macOS with ordinary CMake/CTest. The application itself is Windows-only.

## Architecture

| File | Responsibility |
| --- | --- |
| `src/model.hpp` | Portable state model, validation, versioned binary codec and checksum |
| `src/main.cpp` | Application lifecycle, tray, hotkeys, layout recovery and desktop window ordering |
| `src/panel.cpp` | Group windows, OLE file drops, launch actions and folder notifications |
| `src/render.cpp` | GDI+ renderer, per-pixel alpha, wallpaper sampling and materials |
| `src/settings.cpp` | Native settings UI and live adjustments |
| `src/platform.cpp` | UTF-8/UTF-16 conversion, file dialogs, atomic writes and login startup |

No web view, Electron, Tauri, network service or external UI framework. C++20, Win32, GDI+, COM and CMake. Configuration lives at `%LOCALAPPDATA%\FencesT\layout.fencest` with a `.bak` backup. Optional startup uses the current user's `Run` registry entry; disable it before moving the executable.
