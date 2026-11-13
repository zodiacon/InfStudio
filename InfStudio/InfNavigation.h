#pragma once

#include <string>
#include <string_view>
#include <optional>

//
// Going from what is at a place in an INF's text (UTF-8, as the editor has it) to where it's defined, by the lexer's
// reading of it (so comments and strings are what they are).
//
namespace InfNavigation {
	// What the text at pos refers to, defined elsewhere in it: a string key (%key%, in a value or a string) -> its line
	// in [Strings] (or, if it isn't there, in a localized [Strings.xxxx]); a section name (a field of a value, or a key:
	// [DestinationDirs]) -> its [header]. The start of that line or header; npos if there's nothing to go to.
	size_t FindDefinition(std::string_view text, size_t pos);
	// the value of the string key at pos (its quotes taken off, "" a quote, without a comment), if it's one that's defined
	std::optional<std::string> StringValueAt(std::string_view text, size_t pos);
}
