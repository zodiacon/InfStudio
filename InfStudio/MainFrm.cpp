// MainFrm.cpp : implementation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "InfView.h"
#include "MainView.h"
#include "MainFrm.h"
#include "ToolbarHelper.h"
#include "SecurityHelper.h"
#include "IconHelper.h"
#include "ThemeHelper.h"

const int WINDOW_MENU_POSITION = 5;

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto loaded = s_Settings.LoadFromKey(L"SOFTWARE\\ScorpioSoftware\\InfStudio");
	if (loaded) {
		s_recentFiles.Set(s_Settings.RecentFiles());
	}
	CreateSimpleStatusBar();
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	int parts[] = { 200, 400, 600, 800, 1000 };
	m_StatusBar.SetParts(_countof(parts), parts);

	ToolBarButtonInfo const buttons[] = {
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ 0 },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ 0 },
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
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	m_view.SetWindowMenu(hMenu.GetSubMenu(WINDOW_MENU_POSITION));

	WCHAR path[MAX_PATH];
	::GetWindowsDirectory(path, _countof(path));
	::wcscat_s(path, L"\\INF\\");
	WIN32_FIND_DATA fd;
	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 1, 1);
	if (INVALID_HANDLE_VALUE != ::FindFirstFile(path + CString(L"*.inf"), &fd)) {
		WORD icon = 0;
		wcscat_s(path, fd.cFileName);
		auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), path, &icon);
		if (hIcon) {
			images.AddIcon(hIcon);
			m_InfIcon.Attach(hIcon);
		}
	}
	m_view.SetImageList(images);

	InitMenu();
	SetAlwaysOnTop();
	UpdateRecentFilesMenu();

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

void CMainFrame::InitMenu() {
	struct {
		int id;
		UINT icon;
		HICON hIcon{ nullptr };
	} const commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_UNDO, IDI_UNDO },
		{ ID_EDIT_REDO, IDI_REDO },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_FILE_SAVE_AS, IDI_SAVE_AS },
		{ ID_OPTIONS_ALWAYSONTOP, IDI_PIN },
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
	};
	for (auto& cmd : commands) {
		if (cmd.icon)
			AddCommand(cmd.id, cmd.icon);
		else
			AddCommand(cmd.id, cmd.hIcon);
	}
	AddMenu(GetMenu());
	UIAddMenu(GetMenu());
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	s_Settings.Save();

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
	auto pView = new CInfView;
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_view.AddPage(pView->m_hWnd, L"(Untitled)", -1, pView);

	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(TRUE, L"inf", nullptr, OFN_EXPLORER | OFN_ENABLESIZING, L"INF Files\0*.inf\0");
	ThemeHelper::Suspend();
	if (IDOK == dlg.DoModal()) {
		DoFileOpen(dlg.m_szFileName, dlg.m_szFileTitle);
	}
	ThemeHelper::Resume();

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
		auto view = (IView*)m_view.GetPageData(nActivePage);
		ATLASSERT(view);
		if (view->CanClose()) {
			m_view.RemovePage(nActivePage);
		}
	}
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();

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
	return ShowContextMenu(hMenu, flags, x, y);
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
		((IView*)m_view.GetPageData(m_CurrentPage))->PageActivated(false);
	}
	if (page >= 0) {
		auto view = (IView*)m_view.GetPageData(page);
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
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, s_Settings.AlwaysOnTop());
	SetWindowPos(s_Settings.AlwaysOnTop() ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD, HWND, BOOL&) {
	s_Settings.AlwaysOnTop(!s_Settings.AlwaysOnTop());
	SetAlwaysOnTop();
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
		m_view.AddPage(pView->m_hWnd, title, 0, pView);
		s_recentFiles.AddFile(path);
		s_Settings.RecentFiles(s_recentFiles.Files());
		UpdateRecentFilesMenu();
		return true;
	}
	else {
		pView->DestroyWindow();
		return false;
	}
}
