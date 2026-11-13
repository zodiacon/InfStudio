// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "InfView.h"
#include "InfLexer.h"
#include "EditorFont.h"
#include "AppSettings.h"
#include "TextFile.h"
#include <DpiHelper.h>

namespace {
	// the colors of the editor in one mode (the styles are InfLexer's)
	struct EditorColors {
		COLORREF Back, Text, Comment, Section, Key, Operator, String, Token, Number;
		COLORREF Caret, Selection, CaretLine, MarginBack, MarginText;
		COLORREF ErrorMark, WarningMark;
		COLORREF TipBack, TipText;
	};

	// the indicators that mark InfVerif's problems
	constexpr int ErrorIndicator = 8, WarningIndicator = 9;

	constexpr EditorColors LightColors{
		RGB(255, 255, 255), RGB(0, 0, 0), RGB(0, 128, 0), RGB(160, 0, 0), RGB(0, 0, 160), RGB(90, 90, 90), RGB(0, 128, 128), RGB(128, 0, 128), RGB(152, 104, 1),
		RGB(0, 0, 0), RGB(173, 214, 255), RGB(242, 246, 252), RGB(240, 240, 240), RGB(130, 130, 130),
		RGB(228, 0, 0), RGB(200, 140, 0),
		RGB(255, 255, 225), RGB(0, 0, 0),
	};

	constexpr EditorColors DarkColors{
		RGB(30, 30, 30), RGB(212, 212, 212), RGB(106, 153, 85), RGB(86, 156, 214), RGB(156, 220, 254), RGB(170, 170, 170), RGB(206, 145, 120), RGB(197, 134, 192), RGB(181, 206, 168),
		RGB(220, 220, 220), RGB(38, 79, 120), RGB(42, 45, 46), RGB(37, 37, 38), RGB(133, 133, 133),
		RGB(241, 76, 76), RGB(205, 173, 0),
		RGB(45, 45, 48), RGB(220, 220, 220),
	};
}

LRESULT CInfView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	Focus();
	return 0;
}

LRESULT CInfView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	DefWindowProc();
	// the document is UTF-8 (TextFile converts the file's encoding to and from it)
	Execute(SCI_SETCODEPAGE, SC_CP_UTF8);
	SetLexer(InfLexer::Create());
	// line numbers in margin 0
	Execute(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
	// sections fold (InfLexer sets the levels) in margin 1
	Execute(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
	Execute(SCI_SETMARGINMASKN, 1, SC_MASK_FOLDERS);
	Execute(SCI_SETMARGINSENSITIVEN, 1, TRUE);
	static constexpr std::pair<int, int> markers[] = {
		{ SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS }, { SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS },
		{ SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE }, { SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER },
		{ SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED }, { SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED },
		{ SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER },
	};
	for (auto [number, symbol] : markers)
		Execute(SCI_MARKERDEFINE, number, symbol);
	Execute(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);
	// hovering (SCN_DWELLSTART): a string key's value
	Execute(SCI_SETMOUSEDWELLTIME, 500);
	// auto-complete (InfCompletion): one item a line, sorted ignoring case, filtered as typed ignoring case
	Execute(SCI_AUTOCSETSEPARATOR, '\n');
	Execute(SCI_AUTOCSETIGNORECASE, TRUE);
	Execute(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR, SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);
	Execute(SCI_AUTOCSETORDER, SC_ORDER_PRESORTED);
	Execute(SCI_AUTOCSETMAXHEIGHT, 12);
	ApplyColors();

	return 0;
}

void CInfView::ApplyColors() {
	auto dark = WTLHelper::IsDarkMode();
	auto const& c = dark ? DarkColors : LightColors;
	// Scintilla's own scroll bars follow the window's theme
	::SetWindowTheme(m_hWnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

	auto font = EditorFont::Normalize(AppSettings::Get().Font());
	StyleSetFont(STYLE_DEFAULT, TextFile::ToUtf8(font.lfFaceName).c_str());
	Execute(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, font.lfHeight * 10);
	Execute(SCI_STYLESETBOLD, STYLE_DEFAULT, font.lfWeight >= FW_SEMIBOLD);
	Execute(SCI_STYLESETITALIC, STYLE_DEFAULT, font.lfItalic != 0);
	StyleSetFore(STYLE_DEFAULT, c.Text);
	Execute(SCI_STYLESETBACK, STYLE_DEFAULT, c.Back);
	// every style takes the default's font and colors, the lexer's styles then get their own
	Execute(SCI_STYLECLEARALL);
	StyleSetFore(InfLexer::Comment, c.Comment);
	StyleSetFore(InfLexer::Section, c.Section);
	Execute(SCI_STYLESETBOLD, InfLexer::Section, TRUE);
	StyleSetFore(InfLexer::Key, c.Key);
	StyleSetFore(InfLexer::Operator, c.Operator);
	StyleSetFore(InfLexer::String, c.String);
	StyleSetFore(InfLexer::Token, c.Token);
	StyleSetFore(InfLexer::Number, c.Number);

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

	for (int marker = SC_MARKNUM_FOLDEREND; marker <= SC_MARKNUM_FOLDEROPEN; marker++) {
		Execute(SCI_MARKERSETFORE, marker, c.MarginBack);
		Execute(SCI_MARKERSETBACK, marker, c.MarginText);
	}
	Execute(SCI_CALLTIPSETBACK, c.TipBack);
	Execute(SCI_CALLTIPSETFORE, c.TipText);
	// the auto-complete list (colours with their alpha, opaque)
	auto opaque = [](COLORREF color) { return (LPARAM)(color | 0xFF000000); };
	Execute(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST, opaque(c.Text));
	Execute(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_BACK, opaque(c.TipBack));
	Execute(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED, opaque(c.Text));
	Execute(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED_BACK, opaque(c.Selection));

	Execute(SCI_INDICSETSTYLE, ErrorIndicator, INDIC_SQUIGGLE);
	Execute(SCI_INDICSETFORE, ErrorIndicator, c.ErrorMark);
	Execute(SCI_INDICSETSTYLE, WarningIndicator, INDIC_SQUIGGLE);
	Execute(SCI_INDICSETFORE, WarningIndicator, c.WarningMark);

	UpdateMargins();
	Execute(SCI_COLOURISE, 0, -1);
}

void CInfView::UpdateMargins() {
	Execute(SCI_SETMARGINWIDTHN, 1, DpiHelper::Scale(16, m_hWnd));
	UpdateLineNumberWidth();
}

void CInfView::UpdateLineNumberWidth() {
	auto lines = Execute(SCI_GETLINECOUNT);
	char digits[24] = "_9999";
	for (auto n = lines; n >= 10000; n /= 10)
		strcat_s(digits, "9");
	auto width = (int)Execute(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<LPARAM>(digits));
	Execute(SCI_SETMARGINWIDTHN, 0, width);
}

void CInfView::ShowProblems(std::vector<Problem> const& problems) {
	auto length = Execute(SCI_GETLENGTH);
	for (auto indicator : { ErrorIndicator, WarningIndicator }) {
		Execute(SCI_SETINDICATORCURRENT, indicator);
		Execute(SCI_INDICATORCLEARRANGE, 0, length);
	}
	auto lines = (int)Execute(SCI_GETLINECOUNT);
	for (auto const& problem : problems) {
		if (problem.Line < 1 || problem.Line > lines)
			continue;
		// the line's text, without its indentation
		auto start = Execute(SCI_GETLINEINDENTPOSITION, problem.Line - 1);
		auto end = Execute(SCI_GETLINEENDPOSITION, problem.Line - 1);
		if (end <= start)
			continue;
		Execute(SCI_SETINDICATORCURRENT, problem.Error ? ErrorIndicator : WarningIndicator);
		Execute(SCI_INDICATORFILLRANGE, start, end - start);
	}
}

LRESULT CInfView::OnLButtonDown(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	// (the view chains its messages here: only the editor's own clicks)
	bHandled = FALSE;
	if (GetCurrentMessage()->hwnd != m_hWnd || !(wParam & MK_CONTROL) || !m_CtrlClick)
		return 0;
	auto pos = Execute(SCI_POSITIONFROMPOINTCLOSE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	if (pos >= 0 && m_CtrlClick(pos))
		bHandled = TRUE;
	return 0;
}

LRESULT CInfView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	ApplyColors();
	return 0;
}
