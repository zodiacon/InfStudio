#pragma once

#include <DpiHelper.h>

//
// A window that redoes what depends on its DPI after the DPI changed (it moved to a monitor of another DPI, or its monitor's
// DPI changed): T::OnDpiChanged(dpi). WM_DPICHANGED_AFTERPARENT comes to a window before its children (it goes down the tree),
// so what they do with it - a list view scaling its own font and its columns, a tool bar its buttons - isn't done yet when the
// window gets it. So the window posts itself a message and does its part when that comes, after all of them.
// CHAIN_MSG_MAP(CDpiAware<T>) in the window's message map. OnDpiChanged is called once for each DPI the window ends up at.
//
template<typename T>
class CDpiAware {
public:
	inline static const UINT DpiChangedMessage = ::RegisterWindowMessage(L"InfStudio.DpiChanged");

	BEGIN_MSG_MAP(CDpiAware)
		MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnAfterParent)
		MESSAGE_HANDLER(DpiChangedMessage, OnDpiChangedMessage)
	END_MSG_MAP()

protected:
	// the DPI the window's parts were last made for (0: not known yet, which counts as a change)
	UINT AppliedDpi() const noexcept {
		return m_Dpi;
	}
	void AppliedDpi(UINT dpi) noexcept {
		m_Dpi = dpi;
	}

private:
	LRESULT OnAfterParent(UINT, WPARAM, LPARAM, BOOL& handled) {
		static_cast<T*>(this)->PostMessage(DpiChangedMessage);
		handled = FALSE;
		return 0;
	}

	LRESULT OnDpiChangedMessage(UINT, WPARAM, LPARAM, BOOL&) {
		auto pT = static_cast<T*>(this);
		UINT dpi = DpiHelper::GetDpi(pT->m_hWnd);
		if (dpi != m_Dpi) {
			m_Dpi = dpi;
			// The list views below may not have heard of the change yet even now (it was seen to reach them after this message):
			// they scale their columns (ListViewHelper::EnableColumnDpiScaling) once for each DPI when they do, which would scale
			// widths set by OnDpiChanged again. So they hear of it first.
			::EnumChildWindows(pT->m_hWnd, [](HWND child, LPARAM) -> BOOL {
				WCHAR name[32]{};
				if (::GetClassName(child, name, _countof(name)) && ::_wcsicmp(name, WC_LISTVIEW) == 0)
					::SendMessage(child, WM_DPICHANGED_AFTERPARENT, 0, 0);
				return TRUE;
				}, 0);
			pT->OnDpiChanged(dpi);
		}
		return 0;
	}

	UINT m_Dpi{ 0 };
};
