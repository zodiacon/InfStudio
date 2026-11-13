// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ScintillaCtrl.h"
#include <WTLHelper.h>
#include <functional>

class CInfView :
	public CWindowImpl<CInfView, CScintillaCtrl>,
	public CScintillaCommands<CInfView> {
public:
	DECLARE_WND_CLASS(NULL)

	BEGIN_MSG_MAP(CInfView)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP_ALT(CScintillaCommands<CInfView>, 1)
	END_MSG_MAP()

	// the line number margin wide enough for the document's line count (at least 4 digits)
	void UpdateLineNumberWidth();
	// the margins for the window's DPI (the fold margin's width) and the font's size (the line numbers')
	void UpdateMargins();
	// squiggles under the lines with problems (1-based; error or warning), instead of those there were
	struct Problem {
		int Line;
		bool Error;
	};
	void ShowProblems(std::vector<Problem> const& problems);
	// Ctrl+click: called with the position clicked; true if it went somewhere (then the click isn't Scintilla's,
	// which would add a selection)
	void SetCtrlClickHandler(std::function<bool(LRESULT)> handler) {
		m_CtrlClick = std::move(handler);
	}
	// the font in the settings (Options > Font) again
	void FontChanged() {
		ApplyColors();
	}

private:
	// the editor's colors for the current mode (light or dark); Scintilla draws itself, so dark mode doesn't reach it
	void ApplyColors();
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	std::function<bool(LRESULT)> m_CtrlClick;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};
