#pragma once

#include "ScintillaCtrl.h"
#include <string_view>

//
// Find and replace in the editor (the text is UTF-8, as the document is).
//
struct SearchOptions {
	bool MatchCase{ false };
	bool WholeWord{ false };
	bool Down{ true };
};

namespace EditorSearch {
	// From the selection (after it going down, before it going up), wrapping around the document once; selects and
	// shows what it finds. False if the text isn't in the document.
	bool FindNext(CScintillaCtrl& editor, std::string_view text, SearchOptions const& options);
	// Replaces the selection if it is a match, then finds the next one. False if there's none.
	bool Replace(CScintillaCtrl& editor, std::string_view text, std::string_view replacement, SearchOptions const& options);
	// every match, as one undo action; how many there were
	int ReplaceAll(CScintillaCtrl& editor, std::string_view text, std::string_view replacement, SearchOptions const& options);
}
