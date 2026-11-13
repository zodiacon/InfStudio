# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

INF Studio is a native Windows (C++20, WTL) GUI for viewing and editing driver installation (INF) files. It shows the INF text in a Scintilla editor next to a tree that breaks down its sections (Version, Manufacturer/Models, AddReg, CopyFiles, ...). Its UI follows the author's other WTL tools (e.g. AstroStudio), all built on the shared `WTLHelper` library.

## Build

Visual Studio solution (`InfStudio.sln`), toolset v145 (VS 18: `C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe`), `stdcpp20`, static CRT (`/MT`, `/MTd`) everywhere:

```
msbuild InfStudio.sln /p:Configuration=Debug /p:Platform=x64 /m
msbuild InfStudio.sln /p:Configuration=Release /p:Platform=x64 /m
```

- x64 is the platform that matters; the solution's ARM64 maps to x64.
- Dependencies come from vcpkg's user-wide integration (no manifest in the repo). `InfStudio.vcxproj` sets `VcpkgUseStatic`, so it takes the `x64-windows-static` triplet: `scintilla`, `lexilla` (both only installed static), plus `detours` and `wil` that WTLHelper uses. Static Scintilla needs `imm32` (linked by a `#pragma comment` in `InfStudio.cpp`).
- WTL comes from the headers vendored in `WTLHelper\WTL` (on the include path). Nothing uses NuGet anymore.
- `WTLHelper` is a git submodule (`git submodule update --init`). It is the author's shared library with its own repo and its own `CLAUDE.md`, so changes there are a commit inside the submodule and affect other apps.
- The `lexilla/` folder in the repo is no longer built (Lexilla comes from vcpkg); it is leftover source.
- There is no test project. `DumpInf` is a console app that prints an INF's version info and sections through `InfCore` (no argument: the master INF), for checking `InfFile` without the GUI. The GUI accepts INF paths on its command line, which makes it easy to launch on a file.

## Architecture

Projects in the solution:

- **InfCore** (static lib): `InfFile` wraps the SetupAPI INF parser (`SetupOpenInfFile`, `SetupFindFirstLine`, `SetupEnumInfSections`, ...). All INF parsing goes through SetupAPI; there is no custom parser, so a parse error is only a SetupAPI error line number.
- **InfStudio** (GUI exe): depends on InfCore and WTLHelper, and on Scintilla/Lexilla from vcpkg (`<scintilla/...>`, `<lexilla/...>` in `pch.h`).
- **DumpInf** (console exe): depends on InfCore.

How the GUI is put together:

- `wWinMain` (`InfStudio.cpp`) loads `AppSettings` from the registry (`AppSettings::RegistryKey`, HKCU) before any window exists, starts dark mode (`WTLHelper::InitDarkMode`: the saved choice, or the system's while `DarkMode` is -1), registers Scintilla's window classes, and restores the main window's saved placement. `CMainFrame::OnDestroy` saves the settings.
- `CMainFrame` (`MainFrm.*`) is the frame: rebar tool bar (`ToolbarHelper`), status bar, and a `CNativeCustomTabView` with one `CMainView` per file. Each page's data is the view's `IView*` (`ViewOfPage`); closing a tab, Close All and closing the app ask each view's `CanClose`. Views reach the frame only through `IMainFrame` (`Interfaces.h`). Menu icons are set with `WTLHelper::InitMenu`, again after a mode switch and for every context menu (`TrackPopupMenu`), since the bitmaps are drawn for the current background.
- Dark mode: Options > Dark Mode calls `WTLHelper::SwitchToMode`, which themes the standard controls and sends the registered `WTLHelper::ThemeChangedMessage` to every descendant. Scintilla draws itself, so `CInfView` handles that message and re-applies its own light/dark palette and scroll-bar theme (`ApplyColors`). A window that paints itself needs the same. Wrap common dialogs (file open/save) in `WTLHelper::SuspendHook`/`ResumeHook`.
- `CViewBase<T>` (`ViewBase.h`) is the base for tab views: a `CFrameWindowImpl` child that deletes itself in `OnFinalMessage`, forwards `WM_FORWARDMSG` to `PreTranslateMessage` and calls `T::OnActivated` when its tab is activated. Commands the frame doesn't handle itself go to the active page.
- `CMainView` holds a splitter with a tree (`CTreeViewHelper`) and a `CInfView` (Scintilla via `ScintillaCtrl.h`, lexer `CreateLexer("props")`). In `ScintillaCtrl.h` only the methods with inline bodies exist; the rest are declarations without definitions, so call `Execute(SCI_...)` for anything else. `BuildTree`/`AnalyzeAndBuild` open the file through `InfFile` and fill the tree. Each tree item stores a `NodeType`, and its icon is a `TreeIconIndex`, so a new kind of known section means adding to both enums and the image list. The analysis always reads the file on disk, not the editor buffer: `BuildTree` skips it while the editor has unsaved changes, and Refresh (`ID_VIEW_REFRESH`) saves the file before it rebuilds the tree.
- `AppSettings` uses WTLHelper's `Settings` macros. A new setting needs both its `SETTING` entry and its `DEF_SETTING` accessor.
- Precompiled headers: each project has its own `pch.h`, which every `.cpp` must include first.
