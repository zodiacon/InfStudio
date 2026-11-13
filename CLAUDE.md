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
- `DumpInf` is a console app that prints an INF's version info and sections through `InfCore` (no argument: the master INF). The GUI accepts INF paths on its command line, which makes it easy to launch on a file.

### Tests

`InfStudio.Tests` is a Catch2 v3 console project (x64, Debug and Release). Catch2 comes from vcpkg's static triplet with auto-linking off, so the project lists the libraries it links (Catch2 and Lexilla). It compiles `InfLexer.cpp` and `TextFile.cpp` from `InfStudio/` directly and links `InfCore`. Build the solution, then run:

```
x64\Debug\InfStudio.Tests.exe              # all tests; exit code 0 when everything passes
x64\Debug\InfStudio.Tests.exe [lexer]      # by tag: [lexer], [scintilla], [textfile], [inffile]
x64\Debug\InfStudio.Tests.exe "Strings"    # one test case by name
```

- `InfLexerTests.cpp` checks `InfLexer::StyleLine` with one letter per byte (`.` default, `c` comment, `S` section, `k` key, `o` operator, `s` string, `t` string key, `n` number), so an expected value lines up under its line. The `[scintilla]` tests run the real lexer (`InfLexer::Create`) over a `FakeDocument` (an `IDocument` on a string). They cover what only the Scintilla side does: line ends, the continuation in the line state, lexing from the middle of a document.
- `TextFileTests.cpp` and `InfFileTests.cpp` work on files in the temporary directory (`TempFile` in `TestCommon.h`). `InfFile` is tested through the real SetupAPI. Expectations that depend on the ANSI code page run only under code page 1252.
- `InfTemplatesTests.cpp` generates every template and checks it: placeholders filled, CRLF, ASCII, SetupAPI opens it, every `%strkey%` it uses (found with the lexer) is in `[Strings]`, and the Manufacturer's models sections exist. The `[infverif]` test runs the WDK's `infverif.exe /w` (Windows Driver requirements) on each and is skipped when the WDK isn't installed. A new or changed template must pass it.
- Keep the test sources ASCII (use `\u` escapes): they have no BOM, so MSVC reads them in the ANSI code page.

## Architecture

Projects in the solution:

- **InfCore** (static lib): `InfFile` wraps the SetupAPI INF parser (`SetupOpenInfFile`, `SetupFindFirstLine`, `SetupEnumInfSections`, ...). All INF parsing goes through SetupAPI; there is no custom parser, so a parse error is only a SetupAPI error line number.
- **InfStudio** (GUI exe): depends on InfCore and WTLHelper, and on Scintilla/Lexilla from vcpkg (`<scintilla/...>`, `<lexilla/...>` in `pch.h`). Lexilla's own lexers aren't used: the stock "props" lexer doesn't know INF syntax.
- **DumpInf** (console exe): depends on InfCore.

How the GUI is put together:

- `wWinMain` (`InfStudio.cpp`) loads `AppSettings` from the registry (`AppSettings::RegistryKey`, HKCU) before any window exists, starts dark mode (`WTLHelper::InitDarkMode`: the saved choice, or the system's while `DarkMode` is -1), registers Scintilla's window classes, and restores the main window's saved placement. `CMainFrame::OnDestroy` saves the settings.
- `CMainFrame` (`MainFrm.*`) is the frame: rebar tool bar (`ToolbarHelper`), status bar, and a `CNativeCustomTabView` with one `CMainView` per file. Each page's data is the view's `IView*` (`ViewOfPage`); closing a tab, Close All and closing the app ask each view's `CanClose`. Views reach the frame only through `IMainFrame` (`Interfaces.h`). Menu icons are set with `WTLHelper::InitMenu`, again after a mode switch and for every context menu (`TrackPopupMenu`), since the bitmaps are drawn for the current background.
- Dark mode: Options > Dark Mode calls `WTLHelper::SwitchToMode`, which themes the standard controls and sends the registered `WTLHelper::ThemeChangedMessage` to every descendant. Scintilla draws itself, so `CInfView` handles that message and re-applies its own light/dark palette and scroll-bar theme (`ApplyColors`). A window that paints itself needs the same. Wrap common dialogs (file open/save) in `WTLHelper::SuspendHook`/`ResumeHook`.
- `CViewBase<T>` (`ViewBase.h`) is the base for tab views: a `CFrameWindowImpl` child that deletes itself in `OnFinalMessage`, forwards `WM_FORWARDMSG` to `PreTranslateMessage` and calls `T::OnActivated` when its tab is activated. Commands the frame doesn't handle itself go to the active page.
- `CMainView` holds a splitter with a tree (`CTreeViewHelper`) and a `CInfView` (Scintilla via `ScintillaCtrl.h`). In `ScintillaCtrl.h` only the methods with inline bodies exist; the rest are declarations without definitions, so call `Execute(SCI_...)` for anything else. `BuildTree`/`AnalyzeAndBuild` open the file through `InfFile` and fill the tree. Each tree item stores a `NodeType`, and its icon is a `TreeIconIndex`, so a new kind of known section means adding to both enums and the image list. The analysis always reads the file on disk, not the editor buffer: `BuildTree` skips it while the editor has unsaved changes, and Refresh (`ID_VIEW_REFRESH`) saves the file before it rebuilds the tree.
- Syntax highlighting is the app's own lexer, `InfLexer` (`InfLexer.*`): `InfLexer::StyleLine` styles one line (no window needed), and a Lexilla `LexerModule` (`InfLexer::Create`) runs it over whole lines, keeping in each line's state whether the line ends with a `\` continuation (the next line then has no key). Its styles (`InfLexer::Style`) are what `CInfView::ApplyColors` colors; a new style needs a color in both palettes there. A `;` is a comment only outside quotes, `""` is a quote in a string, `%%` a literal percent.
- Text encoding: the editor's document is always UTF-8 (`SCI_SETCODEPAGE`). `TextFile` (`TextFile.*`) loads a file as UTF-8 and saves it back in its own encoding (`TextEncoding`: ANSI without a BOM, UTF-8 or UTF-16 LE with one); `CMainView::ConfirmEncoding` asks to switch an ANSI file to UTF-16 when it has characters the ANSI code page lacks. Text sent to Scintilla (find, section lookup) must be UTF-8 (`TextFile::ToUtf8`), never `CStringA`.
- File > New from Template: the templates are data in `InfTemplates.cpp` (`InfTemplates::All()`; text with `@DATE@` and `@GUID@` placeholders that `Generate` fills with today's date and a new GUID). `CMainFrame::AddTemplatesMenu` builds the submenu from that list at startup (command IDs from `ID_TEMPLATE_FIRST`, a separator between `Group`s), so adding a template is adding an entry. They target Windows 10 1709 (`10.0...16299`; UMDF from the driver store needs 1803, `10.0...17134`) on amd64 and arm64, and copy their files to `DIRID 13`. `CMainView::NewFromText` opens the text as a new file: it has no path and counts as unsaved (`m_Unsaved`, part of `IsDirty`) until Save As writes it, since the editor's own modified flag is clear after `EmptyUndoBuffer`.
- `AppSettings` uses WTLHelper's `Settings` macros. A new setting needs both its `SETTING` entry and its `DEF_SETTING` accessor.
- Precompiled headers: each project has its own `pch.h`, which every `.cpp` must include first.
