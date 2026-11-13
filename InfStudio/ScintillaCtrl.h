#pragma once

template<typename T>
class CScintillaCtrlT : public T {
	using Position = Scintilla::Position;
	using Line = Scintilla::Line;
	using Span = Scintilla::Span;
	using Colour = Scintilla::Colour;
	using ColourAlpha = Scintilla::ColourAlpha;
public:
	HWND Create(HWND hWndParent, _U_RECT rect = nullptr, LPCTSTR szWindowName = nullptr,
		DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, DWORD dwExStyle = 0, _U_MENUorID MenuOrID = 0U, PVOID lpCreateParam = nullptr) {
		return CWindow::Create(GetWndClassName(), hWndParent, rect.m_lpRect, szWindowName, dwStyle, dwExStyle, MenuOrID.m_hMenu, lpCreateParam);
	}

	static LPCTSTR GetWndClassName() {
		return L"Scintilla";
	}

	LRESULT Execute(UINT Msg, WPARAM wParam = 0, LPARAM lParam = 0) const {
		ATLASSERT(::IsWindow(this->m_hWnd));
		return ::SendMessage(this->m_hWnd, Msg, wParam, lParam);
	}

	bool IsSelectionEmpty() const {
		return (bool)Execute(SCI_GETSELECTIONEMPTY);
	}

	int SelectionsCount() const {
		return (int)Execute(SCI_GETSELECTIONS);
	}

void ClearAll() {
		Execute(SCI_CLEARALL);
	}
	
	Position Length() const {
		return (Position)Execute(SCI_GETLENGTH);
	}

	int CharAt(Position pos) const {
		return (int)Execute(SCI_GETCHARAT, pos);
	}
	Position CurrentPos() const {
		return (Position)Execute(SCI_GETCURRENTPOS);
	}
	Position Anchor() const {
		return (Position)Execute(SCI_GETANCHOR);
	}

	int StyleAt(Position pos) const {
		return (int)Execute(SCI_GETSTYLEAT, pos);
	}
	int StyleIndexAt(Position pos) const {
		return (int)Execute(SCI_GETSTYLEINDEXAT, pos);
	}
	void Redo() {
		Execute(SCI_REDO);
	}
	void SetUndoCollection(bool collectUndo) {
		Execute(SCI_SETUNDOCOLLECTION, collectUndo);
	}
	void SelectAll() {
		Execute(SCI_SELECTALL);
	}
	void SetSavePoint() {
		Execute(SCI_SETSAVEPOINT);
	}

	bool CanRedo() const {
		return Execute(SCI_CANREDO);
	}

	void GotoPos(Position caret) {
		Execute(SCI_GOTOPOS, caret);
	}

	void SetCodePage(int codePage) {
		Execute(SCI_SETCODEPAGE, codePage);
	}

	void Focus(bool focus = true) {
		Execute(SCI_SETFOCUS, focus);
	}

void StyleSetFore(int style, Colour fore) {
		Execute(SCI_STYLESETFORE, style, fore);
	}
	void StyleSetItalic(int style, bool italic) {
		Execute(SCI_STYLESETITALIC, style, italic);
	}
	void StyleSetSize(int style, int sizePoints) {
		Execute(SCI_STYLESETSIZE, style, sizePoints);
	}

	void StyleSetFont(int style, const char* fontName) {
		Execute(SCI_STYLESETFONT, style, reinterpret_cast<LPARAM>(fontName));
	}

	void SetKeywords(int index, char const* keywords) {
		Execute(SCI_SETKEYWORDS, index, reinterpret_cast<LPARAM>(keywords));
	}

	void SetLexer(Scintilla::ILexer5* lexer) {
		Execute(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(lexer));
	}

	void StartLexer() {
		Execute(SCI_LEXER_START);
	}

	void StyleResetDefault() {
		Execute(SCI_STYLERESETDEFAULT);
	}
	Colour StyleGetFore(int style) {
		return (Colour)Execute(SCI_STYLEGETFORE, style);
	}

	Position Column(Position pos) const {
		return (Position)Execute(SCI_GETCOLUMN, pos);
	}
	Position LineNumber(Position pos) const {
		return (Position)Execute(SCI_LINEFROMPOSITION, pos);
	}
	bool IsReadOnly() {
		return Execute(SCI_GETREADONLY);
	}
	void SetCurrentPos(Position caret) {
		Execute(SCI_SETCURRENTPOS, caret);
	}
	void SetSelectionStart(Position anchor) {
		Execute(SCI_SETSELECTIONSTART, anchor);
	}
	Position SelectionStart() const {
		return (Position)Execute(SCI_GETSELECTIONNANCHOR);
	}

	void SetSelectionEnd(Position caret) {
		Execute(SCI_SETSELECTIONEND, caret);
	}
	Position FindText(Scintilla::FindOption searchFlags, char const* text) {
		// (the whole document, from the start)
		Sci_TextToFindFull ttf{ };
		ttf.chrg.cpMax = Length();
		ttf.lpstrText = text;
		return (Position)Execute(SCI_FINDTEXTFULL, static_cast<WPARAM>(searchFlags), reinterpret_cast<LPARAM>(&ttf));
	}

	bool IsModified() {
		return Execute(SCI_GETMODIFY);
	}
	Line LineFromPosition(Position pos) const {
		return (Line)Execute(SCI_LINEFROMPOSITION, pos);
	}

	Position PositionFromLine(Line line) const {
		return (Position)Execute(SCI_POSITIONFROMLINE, line);
	}
	void SetReadOnly(bool readOnly) {
		Execute(SCI_SETREADONLY, readOnly);
	}
	bool CanPaste() const {
		return Execute(SCI_CANPASTE);
	}
	bool CanUndo() const {
		return Execute(SCI_CANUNDO);
	}
	void EmptyUndoBuffer() {
		Execute(SCI_EMPTYUNDOBUFFER);
	}

	void Undo() {
		Execute(SCI_UNDO);
	}

	void Cut() {
		Execute(SCI_CUT);
	}

	void Copy() {
		Execute(SCI_COPY);
	}

	void Paste() {
		Execute(SCI_PASTE);
	}

	void Clear() {
		Execute(SCI_CLEAR);
	}

	void SetText(const char* text) {
		Execute(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(text));
	}

	Position GetText(Position length, char* text) const {
		return (Position)Execute(SCI_GETTEXT, length, reinterpret_cast<LPARAM>(text));
	}

	std::string GetText(Position length) const {
		std::string text;
		text.resize((size_t)length);
		GetText(length, text.data());
		return text;
	}
};

using CScintillaCtrl = CScintillaCtrlT<CWindow>;

template<typename T>
class CScintillaCommands {
	BEGIN_MSG_MAP(CScintillaCommands)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnPaste)
		COMMAND_ID_HANDLER(ID_EDIT_CUT, OnCut)
		COMMAND_ID_HANDLER(ID_EDIT_UNDO, OnUndo)
		COMMAND_ID_HANDLER(ID_EDIT_REDO, OnRedo)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, OnClearAll)
		COMMAND_ID_HANDLER(ID_EDIT_DELETE, OnDelete)
	END_MSG_MAP()

	LRESULT OnDelete(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Execute(SCI_CLEAR);
		return 0;
	}

	LRESULT OnPaste(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Paste();
		return 0;
	}

	LRESULT OnCopy(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Copy();
		return 0;
	}

	LRESULT OnCut(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Cut();
		return 0;
	}

	LRESULT OnUndo(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Undo();
		return 0;
	}

	LRESULT OnRedo(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->Redo();
		return 0;
	}

	LRESULT OnClearAll(WORD, WORD, HWND, BOOL&) {
		static_cast<T*>(this)->ClearAll();
		return 0;
	}

};

