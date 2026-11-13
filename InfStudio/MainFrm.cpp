// MainFrm.cpp : implementation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "MainView.h"
#include "MainFrm.h"
#include <ToolbarHelper.h>
#include "SecurityHelper.h"
#include <IconHelper.h>
#include <DpiHelper.h>
#include <WTLHelper.h>

const int WINDOW_MENU_POSITION = 5;

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	if (m_view.PreTranslateMessage(pMsg))
		return TRUE;

	return FALSE;
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	DragAcceptFiles();
	auto& settings = AppSettings::Get();
	s_recentFiles.Set(settings.RecentFiles());

	CreateSimpleStatusBar();
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	int parts[] = { 200, 400, 600, 800, 1000 };
	m_StatusBar.SetParts(_countof(parts), parts);

	ToolBarButtonInfo const buttons[] = {
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ 0 },
		{ ID_EDIT_UNDO, IDI_UNDO },
		{ ID_EDIT_REDO, IDI_REDO },
		{ 0 },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ 0 },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
	};
	CMenuHandle hMenu = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		hMenu.GetSubMenu(0).DeleteMenu(ID_FILE_RUNASADMINISTRATOR, MF_BYCOMMAND);
		hMenu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		CString text;
		GetWindowText(text);
		SetWindowText(text + L" (Administrator)");
	}

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));

	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);

	m_view.m_bTabCloseButton = FALSE;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_view.SetWindowMenu(hMenu.GetSubMenu(WINDOW_MENU_POSITION));

	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	if (!settings.ViewStatusBar()) {
		::ShowWindow(m_hWndStatusBar, SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, 0);
	}

	WCHAR path[MAX_PATH];
	::GetWindowsDirectory(path, _countof(path));
	::wcscat_s(path, L"\\INF\\");
	WIN32_FIND_DATA fd;
	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 1, 1);
	if (auto hFind = ::FindFirstFile(path + CString(L"*.inf"), &fd); hFind != INVALID_HANDLE_VALUE) {
		WORD icon = 0;
		wcscat_s(path, fd.cFileName);
		auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), path, &icon);
		if (hIcon) {
			images.AddIcon(hIcon);
			m_InfIcon.Attach(hIcon);
		}
		::FindClose(hFind);
	}
	m_view.SetImageList(images);

	InitMenu(GetMenu());
	UIAddMenu(GetMenu());
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	SetAlwaysOnTop();
	UpdateRecentFilesMenu();

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	// the files on the command line ("Open with", drag onto the exe)
	for (int i = 1; i < __argc; i++)
		DoFileOpen(__wargv[i]);

	UpdateUI();

	return 0;
}

void CMainFrame::InitMenu(HMENU menu) {
	MenuItemData const commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_UNDO, IDI_UNDO },
		{ ID_EDIT_REDO, IDI_REDO },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_FILE_SAVE_AS, IDI_SAVE_AS },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_OPTIONS_ALWAYSONTOP, IDI_PIN },
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
	};
	// (the bitmaps are made for the current background, so this is done again when the mode changes)
	WTLHelper::InitMenu(menu, commands, _countof(commands), DpiHelper::SmallIconSize(m_hWnd));
}

bool CMainFrame::CanCloseAll() {
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i); view && !view->CanClose())
			return false;
	return true;
}

IView* CMainFrame::ViewOfPage(int page) const {
	return static_cast<IView*>(m_view.GetPageData(page));
}

LRESULT CMainFrame::OnClose(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	// files with unsaved changes get to ask first
	bHandled = !CanCloseAll();
	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	auto& settings = AppSettings::Get();
	settings.SaveWindowPosition(m_hWnd, L"MainWindowPlacement");
	settings.ViewStatusBar(::IsWindowVisible(m_hWndStatusBar) ? 1 : 0);
	settings.Save();

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CMainView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_view.AddPage(pView->m_hWnd, L"(Untitled)", 0, static_cast<IView*>(pView));
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(TRUE, L"inf", nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST, L"INF Files\0*.inf;*.inx\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		DoFileOpen(dlg.m_szFileName, dlg.m_szFileTitle);
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1) {
		auto view = ViewOfPage(nActivePage);
		ATLASSERT(view);
		if (view->CanClose()) {
			m_view.RemovePage(nActivePage);
			if (m_CurrentPage >= m_view.GetPageCount())
				m_CurrentPage = -1;
		}
		UpdateUI();
	}
	else {
		::MessageBeep((UINT)-1);
	}
	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (!CanCloseAll())
		return 0;

	m_CurrentPage = -1;
	while (m_view.GetPageCount() > 0)
		m_view.RemovePage(m_view.GetPageCount() - 1);
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}

HWND CMainFrame::GetHwnd() const {
	return m_hWnd;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
	InitMenu(hMenu);
	return ::TrackPopupMenu(hMenu, flags, x, y, 0, m_hWnd, nullptr);
}

CUpdateUIBase& CMainFrame::GetUI() {
	return *this;
}

bool CMainFrame::AddToolBar(HWND tb) {
	return UIAddToolBar(tb);
}

void CMainFrame::SetStatusText(int index, PCWSTR text) {
	m_StatusBar.SetText(index, text);
}

HICON CMainFrame::GetInfIcon() const {
	return m_InfIcon.m_hIcon;
}

LRESULT CMainFrame::OnPageActivated(int, LPNMHDR hdr, BOOL&) {
	auto page = static_cast<int>(hdr->idFrom);
	if (m_CurrentPage >= 0 && m_CurrentPage < m_view.GetPageCount()) {
		ViewOfPage(m_CurrentPage)->PageActivated(false);
	}
	if (page >= 0) {
		auto view = ViewOfPage(page);
		ATLASSERT(view);
		view->PageActivated(true);
	}
	m_CurrentPage = page;

	return 0;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD, WORD, HWND, BOOL&) {
	if (SecurityHelper::RunElevated(nullptr, true)) {
		SendMessage(WM_CLOSE);
	}

	return 0;
}

void CMainFrame::SetAlwaysOnTop() {
	auto onTop = AppSettings::Get().AlwaysOnTop();
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, onTop);
	SetWindowPos(onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD, HWND, BOOL&) {
	AppSettings::Get().AlwaysOnTop(!AppSettings::Get().AlwaysOnTop());
	SetAlwaysOnTop();
	return 0;
}

LRESULT CMainFrame::OnToggleDarkMode(WORD, WORD, HWND, BOOL&) {
	WTLHelper::SwitchToMode(WTLHelper::IsDarkMode() ? DarkModeKind::Classic : DarkModeKind::Dark, m_hWnd);
	AppSettings::Get().DarkMode(WTLHelper::IsDarkMode() ? 1 : 0);
	InitMenu(GetMenu());
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	return 0;
}

void CMainFrame::UpdateRecentFilesMenu() {
	if (s_recentFiles.IsEmpty())
		return;

	auto menu = ((CMenuHandle)GetMenu()).GetSubMenu(0);
	CString text;
	int i = 0;
	for (; ; i++) {
		menu.GetMenuString(i, text, MF_BYPOSITION);
		if (text == L"&Recent Files")
			break;
	}
	menu = menu.GetSubMenu(i);
	while (menu.DeleteMenu(0, MF_BYPOSITION))
		;

	i = 0;
	for (auto& file : s_recentFiles.Files()) {
		menu.AppendMenu(MF_BYPOSITION, ATL_IDS_MRU_FILE + i++, file.c_str());
	}
}

LRESULT CMainFrame::OnRecentFile(WORD, WORD id, HWND, BOOL&) {
	DoFileOpen(s_recentFiles.Files()[id - ATL_IDS_MRU_FILE].c_str());
	UpdateUI();
	return 0;
}

bool CMainFrame::DoFileOpen(PCWSTR path, PCWSTR name) {
	CString title(name);
	if (title.IsEmpty()) {
		title = wcsrchr(path, L'\\') + 1;
	}
	auto pView = new CMainView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	CWaitCursor wait;
	if (pView->OpenFile(path)) {
		m_view.AddPage(pView->m_hWnd, title, 0, static_cast<IView*>(pView));
		s_recentFiles.AddFile(path);
		AppSettings::Get().RecentFiles(s_recentFiles.Files());
		UpdateRecentFilesMenu();
		return true;
	}
	else {
		pView->DestroyWindow();
		return false;
	}
}

void CMainFrame::UpdateUI() {
	auto count = m_view.GetPageCount();
	if (count == 0) {
		UINT ids[] = {
			ID_WINDOW_CLOSE, ID_WINDOW_CLOSE_ALL, ID_FILE_CLOSE, ID_FILE_SAVE, ID_FILE_SAVE_AS, ID_EDIT_COPY,
			ID_EDIT_PASTE, ID_EDIT_CUT, ID_EDIT_UNDO, ID_EDIT_REDO, ID_VIEW_REFRESH
		};
		for (auto id : ids)
			UIEnable(id, false);
	}
	else {
		UIEnable(ID_WINDOW_CLOSE, true);
		UIEnable(ID_WINDOW_CLOSE_ALL, true);
		UIEnable(ID_FILE_CLOSE, true);
		UIEnable(ID_FILE_SAVE_AS, true);
	}
}

bool CMainFrame::UpdateTabTitle(HWND tab, PCWSTR title) {
	auto count = m_view.GetPageCount();
	for (int i = 0; i < count; i++)
		if (m_view.GetPageHWND(i) == tab) {
			m_view.SetPageTitle(i, title);
			return true;
		}
	return false;
}

LRESULT CMainFrame::OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto hDrop = (HDROP)wParam;
	WCHAR path[MAX_PATH];
	if (::DragQueryFile(hDrop, 0, path, _countof(path)))
		DoFileOpen(path);
	::DragFinish(hDrop);
	UpdateUI();
	return 0;
}
