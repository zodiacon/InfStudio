// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "InfView.h"

namespace {
	// the colors of the editor in one mode; the lexer is Lexilla's "props" (INI-like), which suits INF files
	struct EditorColors {
		COLORREF Back, Text, Comment, Section, Assignment, DefVal, Key;
		COLORREF Caret, Selection, CaretLine, MarginBack, MarginText;
	};

	constexpr EditorColors LightColors{
		RGB(255, 255, 255), RGB(0, 0, 0), RGB(0, 128, 0), RGB(160, 0, 0), RGB(0, 0, 255), RGB(255, 0, 255), RGB(0, 0, 128),
		RGB(0, 0, 0), RGB(173, 214, 255), RGB(242, 246, 252), RGB(240, 240, 240), RGB(130, 130, 130),
	};

	constexpr EditorColors DarkColors{
		RGB(30, 30, 30), RGB(212, 212, 212), RGB(106, 153, 85), RGB(86, 156, 214), RGB(180, 180, 180), RGB(206, 145, 120), RGB(156, 220, 254),
		RGB(220, 220, 220), RGB(38, 79, 120), RGB(42, 45, 46), RGB(37, 37, 38), RGB(133, 133, 133),
	};
}

LRESULT CInfView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	Focus();
	return 0;
}

LRESULT CInfView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	DefWindowProc();
	SetLexer(static_cast<Scintilla::ILexer5*>(CreateLexer("props")));
	// line numbers in margin 0
	Execute(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
	ApplyColors();

	return 0;
}

void CInfView::ApplyColors() {
	auto dark = WTLHelper::IsDarkMode();
	auto const& c = dark ? DarkColors : LightColors;
	// Scintilla's own scroll bars follow the window's theme
	::SetWindowTheme(m_hWnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

	StyleSetFont(STYLE_DEFAULT, "Consolas");
	StyleSetSize(STYLE_DEFAULT, 11);
	StyleSetFore(STYLE_DEFAULT, c.Text);
	Execute(SCI_STYLESETBACK, STYLE_DEFAULT, c.Back);
	// every style takes the default's font and colors, the lexer's styles then get their own
	Execute(SCI_STYLECLEARALL);
	StyleSetFore(SCE_PROPS_COMMENT, c.Comment);
	StyleSetFore(SCE_PROPS_SECTION, c.Section);
	Execute(SCI_STYLESETBOLD, SCE_PROPS_SECTION, TRUE);
	StyleSetFore(SCE_PROPS_ASSIGNMENT, c.Assignment);
	StyleSetFore(SCE_PROPS_DEFVAL, c.DefVal);
	StyleSetFore(SCE_PROPS_KEY, c.Key);

	StyleSetFore(STYLE_LINENUMBER, c.MarginText);
	Execute(SCI_STYLESETBACK, STYLE_LINENUMBER, c.MarginBack);
	Execute(SCI_SETCARETFORE, c.Caret);
	Execute(SCI_SETSELBACK, TRUE, c.Selection);
	Execute(SCI_SETCARETLINEVISIBLE, TRUE);
	Execute(SCI_SETCARETLINEVISIBLEALWAYS, TRUE);
	Execute(SCI_SETCARETLINEBACK, c.CaretLine);
	// the fold margin (1) isn't used: the same color as the line numbers'
	Execute(SCI_SETFOLDMARGINCOLOUR, TRUE, c.MarginBack);
	Execute(SCI_SETFOLDMARGINHICOLOUR, TRUE, c.MarginBack);

	UpdateLineNumberWidth();
	Execute(SCI_COLOURISE, 0, -1);
}

void CInfView::UpdateLineNumberWidth() {
	auto lines = Execute(SCI_GETLINECOUNT);
	char digits[24] = "_9999";
	for (auto n = lines; n >= 10000; n /= 10)
		strcat_s(digits, "9");
	auto width = (int)Execute(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<LPARAM>(digits));
	Execute(SCI_SETMARGINWIDTHN, 0, width);
}

LRESULT CInfView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	ApplyColors();
	return 0;
}
