#pragma once

struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual CUpdateUIBase& GetUI() = 0;
	virtual bool AddToolBar(HWND tb) = 0;
	virtual void SetStatusText(int index, PCWSTR text) = 0;
	virtual HICON GetInfIcon() const = 0;
	virtual bool UpdateTabTitle(HWND tab, PCWSTR title) = 0;
	// a file in a tab of its own (and in the recent files); false if it couldn't be opened
	virtual bool OpenFile(PCWSTR path) = 0;
	// the file's tab (opened if it isn't), at a section if one is given (IView::GoToSection)
	virtual bool OpenFileAt(PCWSTR path, PCWSTR section) = 0;
};

struct IView {
	virtual ~IView() = default;
	virtual void PageActivated(bool active) {}
	virtual bool CanClose() {
		return true;
	}
	// Options > Font changed the editor's font (in the settings)
	virtual void FontChanged() {}
	// Options > Live Analysis was turned on or off (in the settings)
	virtual void LiveAnalysisChanged() {}
	// the app is active again: the file the tab shows may have changed on disk
	virtual void CheckFileChanged() {}
	// the file the tab shows (empty if none, or not saved yet)
	virtual CString FilePath() const {
		return L"";
	}
	// the caret to a section's header; false if there's none of that name
	virtual bool GoToSection(PCWSTR /*name*/) {
		return false;
	}
};
