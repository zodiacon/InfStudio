// InfStudio.cpp : main source file for InfStudio.exe
//

#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include "AppSettings.h"
#include <WTLHelper.h>

// the static Scintilla from vcpkg uses the IME API
#pragma comment(lib, "imm32")

CAppModule _Module;
AppSettings g_Settings;

int Run(LPCTSTR /*lpstrCmdLine*/ = nullptr, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	// where the window was when the program was last closed, if that place is still on a screen
	auto wp = AppSettings::Get().MainWindowPlacement();
	if (wp.length == sizeof(wp) && ::MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONULL)) {
		if (wp.showCmd == SW_SHOWMINIMIZED) {
			wp.showCmd = SW_SHOWNORMAL;
			AppSettings::Get().MainWindowPlacement(wp);
		}
		AppSettings::Get().LoadWindowPosition(wndMain, L"MainWindowPlacement");
	}
	else {
		wndMain.ShowWindow(nCmdShow);
	}

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	HRESULT hRes = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES | ICC_COOL_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES);

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	AppSettings::Get().LoadFromKey(AppSettings::RegistryKey);

	// dark or not as the user last chose, and as the system is until then
	int dark = AppSettings::Get().DarkMode();
	if (dark < 0)
		WTLHelper::InitDarkMode();
	else
		WTLHelper::InitDarkMode(dark ? DarkModeKind::Dark : DarkModeKind::Classic);

	Scintilla_RegisterClasses(_Module.GetModuleInstance());

	int nRet = Run(lpstrCmdLine, nCmdShow);

	Scintilla_ReleaseResources();
	_Module.Term();
	::CoUninitialize();

	return nRet;
}
