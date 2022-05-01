#pragma once

struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual CUpdateUIBase& GetUI() = 0;
	virtual bool AddToolBar(HWND tb) = 0;
	virtual void SetStatusText(int index, PCWSTR text) = 0;
};
