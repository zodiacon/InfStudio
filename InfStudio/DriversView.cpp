#include "pch.h"
#include "DriversView.h"
#include <ListViewhelper.h>
#include <algorithm>
#include <cwctype>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi")

namespace {
	// a "1.2.10" before a "1.10.0": number by number
	int CompareVersions(std::wstring const& a, std::wstring const& b) {
		size_t i = 0, j = 0;
		while (i < a.size() || j < b.size()) {
			unsigned long long x = 0, y = 0;
			while (i < a.size() && a[i] != L'.')
				x = x * 10 + (iswdigit(a[i]) ? a[i] - L'0' : 0), i++;
			while (j < b.size() && b[j] != L'.')
				y = y * 10 + (iswdigit(b[j]) ? b[j] - L'0' : 0), j++;
			if (x != y)
				return x < y ? -1 : 1;
			i++, j++;
		}
		return 0;
	}

	bool ContainsNoCase(std::wstring const& text, std::wstring const& part) {
		return std::search(text.begin(), text.end(), part.begin(), part.end(),
			[](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); }) != text.end();
	}
}

BOOL CDriversView::PreTranslateMessage(MSG*) {
	return FALSE;
}

void CDriversView::OnActivated(bool active) {
	if (!active)
		return;
	// (the commands of a file's tab don't apply here)
	for (UINT id : { ID_FILE_SAVE, ID_FILE_SAVE_AS, ID_EDIT_COPY, ID_EDIT_CUT, ID_EDIT_PASTE, ID_EDIT_UNDO, ID_EDIT_REDO,
		ID_EDIT_DELETE, ID_EDIT_FIND, ID_EDIT_REPLACE, ID_EDIT_REPEAT, ID_EDIT_FIND_PREVIOUS, ID_INF_VALIDATE, ID_INF_VALIDATE_WHQL,
		ID_VIEW_RESULTS, ID_EDIT_GOTO_DEFINITION, ID_EDIT_GO_BACK, ID_EDIT_COMPLETE, ID_ENCODING_ANSI, ID_ENCODING_UTF8, ID_ENCODING_UTF16 })
		UI().UIEnable(id, false);
	UI().UIEnable(ID_VIEW_REFRESH, true);
	Frame()->SetStatusText(0, std::format(L"{} driver packages", m_Shown.size()).c_str());
	for (int pane = 1; pane <= 4; pane++)
		Frame()->SetStatusText(pane, L"");
}

LRESULT CDriversView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Filter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_FILTER);
	m_Filter.SetCueBannerText(L"Filter (name, provider, class, version)", TRUE);
	m_Inbox.Create(m_hWnd, rcDefault, L"Show inbox drivers", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, IDC_INBOX);
	m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		0, IDC_LIST);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_HEADERDRAGDROP);

	// (widths at 96 DPI, scaled; ListViewHelper scales them when the DPI changes)
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	static const struct {
		PCWSTR Title;
		int Width, Format;
	} columns[] = {
		{ L"Name", 110, LVCFMT_LEFT }, { L"Original Name", 170, LVCFMT_LEFT }, { L"Provider", 170, LVCFMT_LEFT },
		{ L"Class", 110, LVCFMT_LEFT }, { L"Date", 85, LVCFMT_LEFT }, { L"Version", 120, LVCFMT_LEFT },
		{ L"Driver Store Folder", 330, LVCFMT_LEFT },
	};
	for (int i = 0; i < _countof(columns); i++)
		m_List.InsertColumn(i, columns[i].Title, columns[i].Format, DpiHelper::Scale(columns[i].Width, dpi));
	ListViewHelper::EnableColumnDpiScaling(m_List);

	ApplyDpi();
	AppliedDpi(dpi);
	Load();
	return 0;
}

void CDriversView::ApplyDpi() {
	CFont font(DpiHelper::CreateMessageFont(DpiHelper::GetDpi(m_hWnd)));
	for (HWND child : { m_Filter.m_hWnd, m_Inbox.m_hWnd, m_List.m_hWnd })
		::SendMessage(child, WM_SETFONT, (WPARAM)font.m_hFont, TRUE);
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.Attach(font.Detach());
	Layout();
}

void CDriversView::OnDpiChanged(UINT) {
	ApplyDpi();
}

void CDriversView::Layout() {
	if (!m_List)
		return;
	CRect rc;
	GetClientRect(&rc);
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	auto scale = [&](int value) { return DpiHelper::Scale(value, dpi); };
	// a row as tall as the font, with room around it
	CClientDC dc(m_hWnd);
	auto old = dc.SelectFont(m_Font);
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	dc.SelectFont(old);
	int height = tm.tmHeight + scale(8), margin = scale(4);
	m_Filter.SetWindowPos(nullptr, margin, margin, scale(320), height, SWP_NOZORDER | SWP_NOACTIVATE);
	m_Inbox.SetWindowPos(nullptr, margin * 3 + scale(320), margin, scale(200), height, SWP_NOZORDER | SWP_NOACTIVATE);
	int top = margin * 2 + height;
	m_List.SetWindowPos(nullptr, 0, top, rc.Width(), std::max(0, rc.Height() - top), SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CDriversView::OnEraseBackground(UINT, WPARAM wParam, LPARAM, BOOL&) {
	CRect rc;
	GetClientRect(&rc);
	auto brush = WTLHelper::IsDarkMode() ? DarkMode::getDlgBackgroundBrush() : ::GetSysColorBrush(COLOR_BTNFACE);
	::FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
	return 1;
}

LRESULT CDriversView::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL& bHandled) {
	if (!WTLHelper::IsDarkMode()) {
		bHandled = FALSE;
		return 0;
	}
	auto dc = reinterpret_cast<HDC>(wParam);
	::SetTextColor(dc, DarkMode::getTextColor());
	::SetBkColor(dc, DarkMode::getDlgBackgroundColor());
	return reinterpret_cast<LRESULT>(DarkMode::getDlgBackgroundBrush());
}

LRESULT CDriversView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
	bHandled = FALSE;
	return 0;
}

LRESULT CDriversView::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	Layout();
	return 0;
}

void CDriversView::Load() {
	m_Loader = std::jthread([hWnd = m_hWnd](std::stop_token stop) {
		auto packages = InstalledDrivers::ReadAll(InstalledDrivers::InfDirectory(), [&](size_t done, size_t count) {
			if (stop.stop_requested())
				return false;
			::PostMessage(hWnd, ProgressMessage, done, count);
			return true;
		});
		// (the view takes them; if it's gone, nobody does)
		auto result = new std::vector<DriverPackage>(std::move(packages));
		if (stop.stop_requested() || !::PostMessage(hWnd, LoadedMessage, 0, reinterpret_cast<LPARAM>(result)))
			delete result;
	});
}

LRESULT CDriversView::OnProgress(UINT, WPARAM done, LPARAM count, BOOL&) {
	Frame()->SetStatusText(0, std::format(L"Reading the driver packages: {} of {}", done, count).c_str());
	return 0;
}

LRESULT CDriversView::OnLoaded(UINT, WPARAM, LPARAM lParam, BOOL&) {
	std::unique_ptr<std::vector<DriverPackage>> packages(reinterpret_cast<std::vector<DriverPackage>*>(lParam));
	m_All = std::move(*packages);
	UpdateShown();
	Frame()->SetStatusText(0, std::format(L"{} driver packages", m_Shown.size()).c_str());
	return 0;
}

LRESULT CDriversView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	if (m_Loader.joinable()) {
		m_Loader.request_stop();
		m_Loader.join();
	}
	Load();
	return 0;
}

LRESULT CDriversView::OnFilterChanged(WORD, WORD, HWND, BOOL&) {
	UpdateShown();
	return 0;
}

LRESULT CDriversView::OnInboxClicked(WORD, WORD, HWND, BOOL&) {
	UpdateShown();
	return 0;
}

std::wstring const& CDriversView::Text(DriverPackage const& p, int column) const {
	switch (column) {
		case Name: return p.PublishedName;
		case OriginalName: return p.OriginalName;
		case Provider: return p.Provider;
		case Class: return p.Class;
		case Date: return p.Date;
		case Version: return p.Version;
	}
	return p.StorePath;
}

void CDriversView::UpdateShown() {
	CString filter;
	m_Filter.GetWindowText(filter);
	std::wstring part = (PCWSTR)filter;
	bool inbox = m_Inbox.GetCheck() == BST_CHECKED;

	m_Shown.clear();
	for (size_t i = 0; i < m_All.size(); i++) {
		auto const& p = m_All[i];
		if (!inbox && !p.ThirdParty)
			continue;
		if (!part.empty() && !ContainsNoCase(p.PublishedName, part) && !ContainsNoCase(p.OriginalName, part) && !ContainsNoCase(p.Provider, part)
			&& !ContainsNoCase(p.Class, part) && !ContainsNoCase(p.Version, part))
			continue;
		m_Shown.push_back(i);
	}
	std::stable_sort(m_Shown.begin(), m_Shown.end(), [&](size_t a, size_t b) {
		auto const& x = m_All[a];
		auto const& y = m_All[b];
		// (text in Explorer's order: oem2 before oem10)
		int result = m_SortColumn == Version ? CompareVersions(x.Version, y.Version) : ::StrCmpLogicalW(Text(x, m_SortColumn).c_str(), Text(y, m_SortColumn).c_str());
		return m_Ascending ? result < 0 : result > 0;
	});
	m_List.SetItemCountEx((int)m_Shown.size(), LVSICF_NOSCROLL);
	m_List.Invalidate();
	if (IsWindowVisible())
		Frame()->SetStatusText(0, std::format(L"{} driver packages", m_Shown.size()).c_str());
}

LRESULT CDriversView::OnGetDispInfo(int, LPNMHDR hdr, BOOL&) {
	auto info = reinterpret_cast<NMLVDISPINFO*>(hdr);
	auto& item = info->item;
	if (!(item.mask & LVIF_TEXT) || item.iItem < 0 || item.iItem >= (int)m_Shown.size())
		return 0;
	auto const& p = m_All[m_Shown[item.iItem]];
	std::wstring_view text = Text(p, item.iSubItem);
	// the store's folder: its name, not the path to it
	std::wstring folder;
	if (item.iSubItem == Store && !p.StorePath.empty()) {
		folder = std::filesystem::path(p.StorePath).parent_path().filename().wstring();
		text = folder;
	}
	wcsncpy_s(item.pszText, item.cchTextMax, text.data(), _TRUNCATE);
	return 0;
}

LRESULT CDriversView::OnColumnClick(int, LPNMHDR hdr, BOOL&) {
	auto column = reinterpret_cast<NMLISTVIEW*>(hdr)->iSubItem;
	m_Ascending = column == m_SortColumn ? !m_Ascending : true;
	m_SortColumn = column;
	UpdateShown();
	return 0;
}

LRESULT CDriversView::OnOpen(int, LPNMHDR, BOOL&) {
	auto selected = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (selected < 0 || selected >= (int)m_Shown.size())
		return 0;
	Frame()->OpenFile(m_All[m_Shown[selected]].Path.c_str());
	return 0;
}
