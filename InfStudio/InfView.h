// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ScintillaCtrl.h"
#include <WTLHelper.h>

class CInfView :
	public CWindowImpl<CInfView, CScintillaCtrl>,
	public CScintillaCommands<CInfView> {
public:
	DECLARE_WND_CLASS(NULL)

	BEGIN_MSG_MAP(CInfView)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP_ALT(CScintillaCommands<CInfView>, 1)
	END_MSG_MAP()

	// the line number margin wide enough for the document's line count (at least 4 digits)
	void UpdateLineNumberWidth();
	// squiggles under the lines with problems (1-based; error or warning), instead of those there were
	struct Problem {
		int Line;
		bool Error;
	};
	void ShowProblems(std::vector<Problem> const& problems);

private:
	// the editor's colors for the current mode (light or dark); Scintilla draws itself, so dark mode doesn't reach it
	void ApplyColors();
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};
