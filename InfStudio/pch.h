// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//  are changed infrequently
//

#pragma once

// Change these values to use different versions
#define WINVER		0x0A00
#define _WIN32_WINNT	0x0A00
#define _WIN32_IE	0x0700
#define _RICHEDIT_VER	0x0500
#define NOMINMAX

#include <atlbase.h>
#include <atlstr.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlx.h>
#include <atlctrlw.h>
#include <atltypes.h>
#include <atlsplit.h>
#include <SetupAPI.h>
#include <commoncontrols.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <format>
#include <unordered_set>
#include <string_view>
#include <wil/resource.h>

// Scintilla and Lexilla come from vcpkg (x64-windows-static)
#include <scintilla/ScintillaTypes.h>
#include <scintilla/ILexer.h>
#include <scintilla/Scintilla.h>
#include <scintilla/ScintillaCall.h>
#include <scintilla/ScintillaMessages.h>
#include <lexilla/Lexilla.h>
#include <lexilla/SciLexer.h>

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
