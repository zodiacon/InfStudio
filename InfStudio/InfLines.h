#pragma once

#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

//
// An INF's text (UTF-8) as lines styled by InfLexer, and what InfNavigation and InfCompletion read from them.
//
namespace InfLines {
	struct StyledLine {
		size_t Start;				// in the text
		std::string_view Text;		// without the line end
		std::vector<uint8_t> Styles;	// InfLexer::Style, one a byte
	};

	// every line of the text (the continuations carried from one to the next, as the lexer does)
	std::vector<StyledLine> StyleLines(std::string_view text);
	// the line pos is on (its end included), or null
	StyledLine const* LineAt(std::vector<StyledLine> const& lines, size_t pos);

	std::string_view Trim(std::string_view text);
	bool EqualNoCase(std::string_view a, std::string_view b);

	// the name of a section header line, if it is one
	std::optional<std::string_view> SectionName(StyledLine const& line);
	// a line's key (before its '='), trimmed; empty if it has none
	std::string_view KeyOf(StyledLine const& line);
	// The fields of a line's value (after its '=', or the whole line if it has none), between its commas, trimmed;
	// to a comment. Offsets are in the line.
	struct Field {
		size_t Start, End;
		std::string_view Text;
	};
	std::vector<Field> ValueFields(StyledLine const& line);
}
