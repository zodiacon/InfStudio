#include "pch.h"
#include "EditorSearch.h"

namespace {
	int Flags(SearchOptions const& options) noexcept {
		return (options.MatchCase ? SCFIND_MATCHCASE : 0) | (options.WholeWord ? SCFIND_WHOLEWORD : 0);
	}

	// the text in [start, end), or backwards from start down to end if end < start; on a match the target is it
	LRESULT Search(CScintillaCtrl& editor, std::string_view text, int flags, LRESULT start, LRESULT end) {
		editor.Execute(SCI_SETSEARCHFLAGS, flags);
		editor.Execute(SCI_SETTARGETRANGE, start, end);
		return editor.Execute(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<LPARAM>(text.data()));
	}

	// whether the selection is a match (what Replace replaces)
	bool SelectionMatches(CScintillaCtrl& editor, std::string_view text, int flags) {
		auto start = editor.Execute(SCI_GETSELECTIONSTART);
		auto end = editor.Execute(SCI_GETSELECTIONEND);
		return start != end && Search(editor, text, flags, start, end) == start && editor.Execute(SCI_GETTARGETEND) == end;
	}
}

bool EditorSearch::FindNext(CScintillaCtrl& editor, std::string_view text, SearchOptions const& options) {
	if (text.empty())
		return false;

	auto flags = Flags(options);
	auto length = editor.Execute(SCI_GETLENGTH);
	auto selStart = editor.Execute(SCI_GETSELECTIONSTART);
	auto selEnd = editor.Execute(SCI_GETSELECTIONEND);
	LRESULT pos;
	if (options.Down) {
		pos = Search(editor, text, flags, selEnd, length);
		if (pos < 0)
			pos = Search(editor, text, flags, 0, selEnd);		// from the top
	}
	else {
		pos = Search(editor, text, flags, selStart, 0);
		if (pos < 0)
			pos = Search(editor, text, flags, length, selStart);	// from the bottom
	}
	if (pos < 0)
		return false;

	editor.Execute(SCI_SETSEL, editor.Execute(SCI_GETTARGETSTART), editor.Execute(SCI_GETTARGETEND));
	editor.Execute(SCI_SCROLLCARET);
	return true;
}

bool EditorSearch::Replace(CScintillaCtrl& editor, std::string_view text, std::string_view replacement, SearchOptions const& options) {
	if (text.empty())
		return false;

	if (SelectionMatches(editor, text, Flags(options))) {
		// (the target is the selection)
		editor.Execute(SCI_REPLACETARGET, replacement.size(), reinterpret_cast<LPARAM>(replacement.data()));
		auto start = editor.Execute(SCI_GETTARGETSTART);
		auto end = editor.Execute(SCI_GETTARGETEND);
		// searching goes on from after the replacement (down) or before it (up)
		editor.Execute(SCI_SETEMPTYSELECTION, options.Down ? end : start);
	}
	return FindNext(editor, text, options);
}

int EditorSearch::ReplaceAll(CScintillaCtrl& editor, std::string_view text, std::string_view replacement, SearchOptions const& options) {
	if (text.empty())
		return 0;

	auto flags = Flags(options);
	int count = 0;
	editor.Execute(SCI_BEGINUNDOACTION);
	for (LRESULT pos = 0; Search(editor, text, flags, pos, editor.Execute(SCI_GETLENGTH)) >= 0; count++) {
		editor.Execute(SCI_REPLACETARGET, replacement.size(), reinterpret_cast<LPARAM>(replacement.data()));
		pos = editor.Execute(SCI_GETTARGETEND);
	}
	editor.Execute(SCI_ENDUNDOACTION);
	return count;
}
