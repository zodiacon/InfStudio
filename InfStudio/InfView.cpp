// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "InfView.h"
#include "LexerModule.h"
#include "SciLexer.h"

extern Lexilla::LexerModule lmProps;

LRESULT CInfView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	Focus();
	return 0;
}

LRESULT CInfView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	DefWindowProc();
	StyleSetFont(STYLE_DEFAULT, "Consolas");
	StyleSetSize(STYLE_DEFAULT, 12);
	auto lex = lmProps.Create();
	SetLexer(lex);
	StyleSetFore(SCE_PROPS_COMMENT, RGB(0, 128, 0));
	StyleSetFore(SCE_PROPS_SECTION, RGB(160, 0, 0));
	StyleSetFore(SCE_PROPS_ASSIGNMENT, RGB(0, 0, 255));
	StyleSetFore(SCE_PROPS_DEFVAL, RGB(255, 0, 255));

	return 0;
}

