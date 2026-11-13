#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "InfLines.h"

//
// What auto-complete offers at a place in an INF's text (UTF-8), by the lexer's reading of it:
// - after a '%' that opens a string key: the keys of [Strings] (and of the localized [Strings.xxxx]), then '%'
// - after a '[' that starts a line: the sections referenced and not there yet, and the standard ones missing, then ']'
// - a key at a line's start: what the section's kind takes ([Version]; an install section a device uses or DefaultInstall,
//   and its .HW, .Services and .Wdf sections; a service section AddService names; a WDF service section; [DestinationDirs]:
//   the file sections), then " = "
// - a field of a value that names sections (CopyFiles, AddReg..., AddService's third, KmdfService's second): the sections
//   there that a directive can name
// - a field of Include: the INFs that can be included (Context::InfNames)
// - a field of Needs: the sections of the INFs the section includes (InfLines::IncludedFiles, read through Context::Included)
//
namespace InfCompletion {
	struct List {
		size_t Start;						// where the word being completed starts (the item replaces it up to the caret)
		std::vector<std::string> Items;		// each once (whatever its case), sorted
		std::string Suffix;					// to add after the chosen item, if it isn't there already
	};

	// what Complete may read beyond the text
	struct Context {
		InfLines::IncludedText Included;		// an included INF's text
		std::vector<std::string> InfNames;		// the INFs Include can name (the file's folder's, %windir%\INF's)
	};

	std::optional<List> Complete(std::string_view text, size_t caret, Context const& context = {});
}
