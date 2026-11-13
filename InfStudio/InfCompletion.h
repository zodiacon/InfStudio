#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

//
// What auto-complete offers at a place in an INF's text (UTF-8), by the lexer's reading of it:
// - after a '%' that opens a string key: the keys of [Strings] (and of the localized [Strings.xxxx]), then '%'
// - after a '[' that starts a line: the sections referenced and not there yet, and the standard ones missing, then ']'
// - a key at a line's start: what the section's kind takes ([Version]; an install section a device uses or DefaultInstall,
//   and its .HW, .Services and .Wdf sections; a service section AddService names; a WDF service section; [DestinationDirs]:
//   the file sections), then " = "
// - a field of a value that names sections (CopyFiles, AddReg..., AddService's third, KmdfService's second; not Needs, which names another INF's):
//   the sections there that a directive can name
//
namespace InfCompletion {
	struct List {
		size_t Start;						// where the word being completed starts (the item replaces it up to the caret)
		std::vector<std::string> Items;		// each once (whatever its case), sorted
		std::string Suffix;					// to add after the chosen item, if it isn't there already
	};

	std::optional<List> Complete(std::string_view text, size_t caret);
}
