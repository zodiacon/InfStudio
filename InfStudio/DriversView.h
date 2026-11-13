#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "DpiAware.h"
#include "..\InfCore\InstalledDrivers.h"
#include <thread>
#include <WTLHelper.h>

//
// File > Browse Installed Drivers: the driver packages Windows has (%windir%\INF), read on a thread of their own; a
// filter, inbox drivers shown or not (a third party's oemNN.inf only, by default), sorted by a column; Enter or a
// double-click opens a package's INF in a tab of its own.
//
class CDriversView :
	public CViewBase<CDriversView>,
	public CDpiAware<CDriversView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);
	void OnActivated(bool active);

	BEGIN_MSG_MAP(CDriversView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		MESSAGE_HANDLER(LoadedMessage, OnLoaded)
		MESSAGE_HANDLER(ProgressMessage, OnProgress)
		COMMAND_HANDLER(IDC_FILTER, EN_CHANGE, OnFilterChanged)
		COMMAND_HANDLER(IDC_INBOX, BN_CLICKED, OnInboxClicked)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		NOTIFY_HANDLER(IDC_LIST, LVN_GETDISPINFO, OnGetDispInfo)
		NOTIFY_HANDLER(IDC_LIST, LVN_COLUMNCLICK, OnColumnClick)
		NOTIFY_HANDLER(IDC_LIST, NM_DBLCLK, OnOpen)
		NOTIFY_HANDLER(IDC_LIST, NM_RETURN, OnOpen)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		COMMAND_RANGE_HANDLER(ID_DRIVER_OPEN, ID_DRIVER_COPYPATH, OnDriverCommand)
		CHAIN_MSG_MAP(CDpiAware<CDriversView>)
		CHAIN_MSG_MAP(CViewBase)
	END_MSG_MAP()

private:
	static constexpr UINT IDC_FILTER = 1101, IDC_INBOX = 1102, IDC_LIST = 1103;
	static constexpr UINT LoadedMessage = WM_APP + 1, ProgressMessage = WM_APP + 2;
	enum Column { Name, OriginalName, Provider, Class, Date, Version, Store, ColumnCount };

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	// the row of the filter in the mode's colours (the window class's background is the light one)
	LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCtlColorStatic(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLoaded(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnInboxClicked(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGetDispInfo(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnColumnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnOpen(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	// A package's menu (right-click, or the menu key / Shift+F10 on the selected one): open its INF or its copy in the
	// driver store, show either in Explorer, copy its row or its path.
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDriverCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the selected package, or null
	DriverPackage const* Selected() const;
	// a row as text: its columns, tab-separated (the store's full path)
	static std::wstring RowText(DriverPackage const& package);

	// reads the packages on a thread; LoadedMessage brings them
	void Load();
	// m_Shown again from m_All: the filter, inbox or not, the sort
	void UpdateShown();
	std::wstring const& Text(DriverPackage const& package, int column) const;
	void Layout();

	friend class CDpiAware<CDriversView>;
	void OnDpiChanged(UINT dpi);
	void ApplyDpi();

	CEdit m_Filter;
	CButton m_Inbox;
	CListViewCtrl m_List;
	CFont m_Font;
	std::vector<DriverPackage> m_All;
	std::vector<size_t> m_Shown;		// indexes in m_All
	int m_SortColumn{ Name };
	bool m_Ascending{ true };
	std::wstring m_StoreFolder;			// (for the display of the Store column)
	std::jthread m_Loader;				// (stopped and joined when the view goes)
};
