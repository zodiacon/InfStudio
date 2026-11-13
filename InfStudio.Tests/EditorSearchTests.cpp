// (the editor control needs ATL/WTL and Scintilla: the app's precompiled header has them)
#include "pch.h"
#include "EditorSearch.h"
#include <catch2/catch_test_macros.hpp>

namespace {
	// a real Scintilla window, hidden
	class Editor {
	public:
		explicit Editor(std::string_view text) {
			static bool registered = Scintilla_RegisterClasses(::GetModuleHandle(nullptr)) != 0;
			REQUIRE(registered);
			m_editor.m_hWnd = ::CreateWindowEx(0, L"Scintilla", nullptr, WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);
			REQUIRE(m_editor.m_hWnd);
			m_editor.Execute(SCI_SETCODEPAGE, SC_CP_UTF8);
			m_editor.SetText(std::string(text).c_str());
			m_editor.Execute(SCI_EMPTYUNDOBUFFER);
		}
		~Editor() {
			m_editor.DestroyWindow();
		}

		CScintillaCtrl& operator*() {
			return m_editor;
		}
		std::pair<LRESULT, LRESULT> Selection() {
			return { m_editor.Execute(SCI_GETSELECTIONSTART), m_editor.Execute(SCI_GETSELECTIONEND) };
		}
		void Caret(LRESULT pos) {
			m_editor.Execute(SCI_SETEMPTYSELECTION, pos);
		}
		std::string Text() {
			return m_editor.GetText(m_editor.Execute(SCI_GETLENGTH));
		}

	private:
		CScintillaCtrl m_editor;
	};

	using Range = std::pair<LRESULT, LRESULT>;
	constexpr SearchOptions Down{}, Up{ false, false, false }, MatchCase{ true, false, true }, WholeWord{ false, true, true };
}

TEST_CASE("Find next goes on from the selection and wraps", "[search]") {
	Editor editor("abc ABC abc");
	CHECK(EditorSearch::FindNext(*editor, "abc", Down));
	CHECK(editor.Selection() == Range{ 0, 3 });
	CHECK(EditorSearch::FindNext(*editor, "abc", Down));
	CHECK(editor.Selection() == Range{ 4, 7 });
	CHECK(EditorSearch::FindNext(*editor, "abc", Down));
	CHECK(editor.Selection() == Range{ 8, 11 });
	// past the last one: from the top
	CHECK(EditorSearch::FindNext(*editor, "abc", Down));
	CHECK(editor.Selection() == Range{ 0, 3 });
}

TEST_CASE("Find previous goes back from the selection and wraps", "[search]") {
	Editor editor("abc ABC abc");
	editor.Caret(11);
	CHECK(EditorSearch::FindNext(*editor, "abc", Up));
	CHECK(editor.Selection() == Range{ 8, 11 });
	CHECK(EditorSearch::FindNext(*editor, "abc", Up));
	CHECK(editor.Selection() == Range{ 4, 7 });
	CHECK(EditorSearch::FindNext(*editor, "abc", Up));
	CHECK(editor.Selection() == Range{ 0, 3 });
	// before the first one: from the bottom
	CHECK(EditorSearch::FindNext(*editor, "abc", Up));
	CHECK(editor.Selection() == Range{ 8, 11 });
}

TEST_CASE("Find options", "[search]") {
	Editor editor("abc ABC abc");
	CHECK(EditorSearch::FindNext(*editor, "abc", MatchCase));
	CHECK(editor.Selection() == Range{ 0, 3 });
	CHECK(EditorSearch::FindNext(*editor, "abc", MatchCase));
	CHECK(editor.Selection() == Range{ 8, 11 });

	Editor words("ab abc ab");
	CHECK(EditorSearch::FindNext(*words, "ab", WholeWord));
	CHECK(words.Selection() == Range{ 0, 2 });
	CHECK(EditorSearch::FindNext(*words, "ab", WholeWord));
	CHECK(words.Selection() == Range{ 7, 9 });
}

TEST_CASE("Nothing to find", "[search]") {
	Editor editor("abc");
	editor.Caret(1);
	CHECK_FALSE(EditorSearch::FindNext(*editor, "xyz", Down));
	CHECK_FALSE(EditorSearch::FindNext(*editor, "", Down));
	// the selection stays
	CHECK(editor.Selection() == Range{ 1, 1 });
}

TEST_CASE("Find UTF-8 text", "[search]") {
	Editor editor("caf\xC3\xA9 \xC3\xA9t\xC3\xA9");
	CHECK(EditorSearch::FindNext(*editor, "\xC3\xA9", Down));
	CHECK(editor.Selection() == Range{ 3, 5 });
	CHECK(EditorSearch::FindNext(*editor, "\xC3\xA9", Down));
	CHECK(editor.Selection() == Range{ 6, 8 });
}

TEST_CASE("Replace replaces a selected match, then finds the next", "[search]") {
	Editor editor("abc ABC abc");
	// nothing selected: it only finds
	CHECK(EditorSearch::Replace(*editor, "abc", "X", Down));
	CHECK(editor.Text() == "abc ABC abc");
	CHECK(editor.Selection() == Range{ 0, 3 });
	CHECK(EditorSearch::Replace(*editor, "abc", "X", Down));
	CHECK(editor.Text() == "X ABC abc");
	CHECK(editor.Selection() == Range{ 2, 5 });
	CHECK(EditorSearch::Replace(*editor, "abc", "X", Down));
	CHECK(editor.Selection() == Range{ 4, 7 });
	// the last one: replaced, and there's no next
	CHECK_FALSE(EditorSearch::Replace(*editor, "abc", "X", Down));
	CHECK(editor.Text() == "X X X");
}

TEST_CASE("Replace all", "[search]") {
	Editor editor("abc ABC abc");
	// (a replacement that has the text in it isn't found again)
	CHECK(EditorSearch::ReplaceAll(*editor, "abc", "abcd", Down) == 3);
	CHECK(editor.Text() == "abcd abcd abcd");
	// one undo takes it all back
	(*editor).Execute(SCI_UNDO);
	CHECK(editor.Text() == "abc ABC abc");

	CHECK(EditorSearch::ReplaceAll(*editor, "abc", "", MatchCase) == 2);
	CHECK(editor.Text() == " ABC ");
	CHECK(EditorSearch::ReplaceAll(*editor, "xyz", "q", Down) == 0);
	CHECK(EditorSearch::ReplaceAll(*editor, "", "q", Down) == 0);
}
