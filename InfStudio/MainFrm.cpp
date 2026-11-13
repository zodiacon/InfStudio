// MainFrm.cpp : implementation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "MainView.h"
#include "DriversView.h"
#include "MainFrm.h"
#include <ToolbarHelper.h>
#include "SecurityHelper.h"
#include <IconHelper.h>
#include <DpiHelper.h>
#include <WTLHelper.h>
#include "InfTemplates.h"
#include "EditorFont.h"

const int WINDOW_MENU_POSITION = 5;

namespace {
	const ToolBarButtonInfo ToolBarButtons[] = {
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

	// the rebar band of a tool bar sized again to it (as WTL's AddSimpleReBarBandCtrl sizes it when it adds the band)
	void FitReBarBand(HWND rebar, HWND toolbar) {
		CReBarCtrl rb(rebar);
		for (int i = 0; i < (int)rb.GetBandCount(); i++) {
			REBARBANDINFO band{ sizeof(band), RBBIM_CHILD };
			if (!rb.GetBandInfo(i, &band) || band.hwndChild != toolbar)
				continue;
			int buttons = (int)::SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0);
			if (buttons == 0)
				return;
			band.fMask = RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE;
			RECT rc{};
			::SendMessage(toolbar, TB_GETITEMRECT, buttons - 1, (LPARAM)&rc);
			band.cx = band.cxIdeal = rc.right;
			band.cyMinChild = rc.bottom - rc.top;
			::SendMessage(toolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
			band.cxMinChild = rc.right;
			rb.SetBandInfo(i, &band);
			return;
		}
	}
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	// Keys in a modeless dialog (Find, Replace) are the dialog's: the frame's shortcuts would take Ctrl+V and the like
	// from its text boxes and give them to the editor.
	if (pMsg->hwnd && ::GetAncestor(pMsg->hwnd, GA_ROOT) != m_hWnd)
		return m_view.PreTranslateMessage(pMsg);

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
	CMenuHandle hMenu = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		hMenu.GetSubMenu(0).DeleteMenu(ID_FILE_RUNASADMINISTRATOR, MF_BYCOMMAND);
		hMenu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		CString text;
		GetWindowText(text);
		SetWindowText(text + L" (Administrator)");
	}

	AddTemplatesMenu(hMenu.GetSubMenu(0));

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, ToolBarButtons, _countof(ToolBarButtons));
	m_ToolBar = tb;

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
	if (auto hFind = ::FindFirstFile(path + CString(L"*.inf"), &fd); hFind != INVALID_HANDLE_VALUE) {
		WORD icon = 0;
		wcscat_s(path, fd.cFileName);
		// (the large icon: the image lists make it the small size of their DPI)
		if (auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), path, &icon))
			m_InfIcon.Attach(hIcon);
		::FindClose(hFind);
	}
	ApplyDpi();

	InitMenu(GetMenu());
	UIAddMenu(GetMenu());
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	UISetCheck(ID_OPTIONS_LIVEANALYSIS, settings.LiveAnalysis());
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

void CMainFrame::AddTemplatesMenu(CMenuHandle fileMenu) {
	CMenuHandle templates;
	templates.CreatePopupMenu();
	auto all = InfTemplates::All();
	ATLASSERT(all.size() <= ID_TEMPLATE_LAST - ID_TEMPLATE_FIRST + 1);
	for (size_t i = 0; i < all.size(); i++) {
		if (i > 0 && all[i].Group != all[i - 1].Group)
			templates.AppendMenu(MF_SEPARATOR);
		templates.AppendMenu(MF_STRING, ID_TEMPLATE_FIRST + (UINT)i, all[i].Name);
	}
	// right after New
	int count = fileMenu.GetMenuItemCount();
	int pos = 0;
	while (pos < count && fileMenu.GetMenuItemID(pos) != ID_FILE_NEW)
		pos++;
	fileMenu.InsertMenu(pos < count ? pos + 1 : 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)templates.m_hMenu, L"New from &Template");
}

LRESULT CMainFrame::OnNewFromTemplate(WORD, WORD wID, HWND, BOOL&) {
	auto const& infTemplate = InfTemplates::All()[wID - ID_TEMPLATE_FIRST];
	auto pView = new CMainView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	pView->NewFromText(InfTemplates::Generate(infTemplate, InfTemplates::NewValues()), infTemplate.FileName);
	m_view.AddPage(pView->m_hWnd, CString(infTemplate.FileName) + L" (new)", 0, static_cast<IView*>(pView));
	UpdateUI();
	return 0;
}

LRESULT CMainFrame::OnBrowseDrivers(WORD, WORD, HWND, BOOL&) {
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (dynamic_cast<CDriversView*>(ViewOfPage(i))) {
			m_view.SetActivePage(i);
			return 0;
		}
	auto pView = new CDriversView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_view.AddPage(pView->m_hWnd, L"Installed Drivers", 0, static_cast<IView*>(pView));
	UpdateUI();
	return 0;
}

bool CMainFrame::OpenFile(PCWSTR path) {
	auto ok = DoFileOpen(path);
	UpdateUI();
	return ok;
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
	// (saved now: when the window is destroyed it's hidden already, and so is the status bar by IsWindowVisible)
	AppSettings::Get().ViewStatusBar(bVisible ? 1 : 0);
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

LRESULT CMainFrame::OnOptionsFont(WORD, WORD, HWND, BOOL&) {
	auto lf = EditorFont::ForDialog(AppSettings::Get().Font(), (int)DpiHelper::GetDpi(m_hWnd));
	CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_NOVERTFONTS, nullptr, m_hWnd);
	if (!WTLHelper::InvokeFontDialog(dlg, m_hWnd))
		return 0;

	LOGFONT chosen{};
	dlg.GetCurrentFont(&chosen);
	AppSettings::Get().Font(EditorFont::FromDialog(chosen, dlg.GetSize()));
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i))
			view->FontChanged();
	return 0;
}

void CMainFrame::ApplyDpi() {
	UINT dpi = DpiHelper::GetDpi(m_hWnd);

	// the tabs' icon at the small icon size of the DPI
	auto size = DpiHelper::SmallIconSize(dpi);
	CImageList images;
	images.Create(size, size, ILC_COLOR32 | ILC_MASK, 1, 1);
	if (m_InfIcon)
		images.AddIcon(m_InfIcon);
	CImageList old = m_view.SetImageList(images);
	if (old && old != images)
		old.Destroy();

	// the status bar: its font and its parts (at 96 DPI: 200 pixels each) for the DPI
	if (m_StatusBar.IsWindow()) {
		auto metrics = DpiHelper::GetNonClientMetrics(dpi);
		CFont font;
		font.CreateFontIndirect(&metrics.lfStatusFont);
		m_StatusBar.SetFont(font);
		if (m_StatusFont)
			m_StatusFont.DeleteObject();
		m_StatusFont.Attach(font.Detach());
		int parts[5];
		for (int i = 0; i < _countof(parts); i++)
			parts[i] = DpiHelper::Scale(200 * (i + 1), dpi);
		m_StatusBar.SetParts(_countof(parts), parts);
	}
}

LRESULT CMainFrame::OnDpiChanged(UINT, WPARAM, LPARAM lParam, BOOL&) {
	// (the tool bar's images when it has handled the change itself, which comes after this)
	PostMessage(CDpiAware<CMainFrame>::DpiChangedMessage);
	ApplyDpi();
	InitMenu(GetMenu());
	DrawMenuBar();
	auto rc = reinterpret_cast<RECT const*>(lParam);
	SetWindowPos(nullptr, rc, SWP_NOZORDER | SWP_NOACTIVATE);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnDpiChangedPosted(UINT, WPARAM, LPARAM, BOOL&) {
	// the buttons' images at the DPI, the buttons measured again from them, and the rebar band fitted to them
	ToolbarHelper::UpdateToolBarImages(m_ToolBar, ToolBarButtons, _countof(ToolBarButtons));
	m_ToolBar.SetButtonSize(0, 0);
	m_ToolBar.AutoSize();
	FitReBarBand(m_hWndToolBar, m_ToolBar);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnLiveAnalysis(WORD, WORD, HWND, BOOL&) {
	auto& settings = AppSettings::Get();
	settings.LiveAnalysis(!settings.LiveAnalysis());
	UISetCheck(ID_OPTIONS_LIVEANALYSIS, settings.LiveAnalysis());
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i))
			view->LiveAnalysisChanged();
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
			ID_EDIT_PASTE, ID_EDIT_CUT, ID_EDIT_UNDO, ID_EDIT_REDO, ID_VIEW_REFRESH, ID_EDIT_DELETE,
			ID_EDIT_FIND, ID_EDIT_REPLACE, ID_EDIT_REPEAT, ID_EDIT_FIND_PREVIOUS, ID_INF_VALIDATE, ID_INF_VALIDATE_WHQL, ID_VIEW_RESULTS,
			ID_EDIT_GOTO_DEFINITION, ID_EDIT_GO_BACK, ID_EDIT_COMPLETE
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
