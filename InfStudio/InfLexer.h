#pragma once

#include <string_view>
#include <span>
#include <cstdint>

//
// Syntax highlighting of INF files, by the rules of "General Syntax Rules for INF Files":
// a ';' outside quotes starts a comment anywhere on a line, "..." is a string ("" is a quote in it), %strkey% a string key
// (%% a literal percent), '=' (outside quotes, before the comment) separates the key from the value, ',' separates the
// values, and a '\' as the last non-blank character continues the line on the next (which then has no key).
//
namespace InfLexer {
	enum Style : uint8_t {
		Default,
		Comment,
		Section,
		Key,
		Operator,	// = , and the continuation '\'
		String,
		Token,		// %strkey%
		Number,		// a value that is a whole decimal or 0x hex number
		StyleCount
	};

	// Styles one line (its text without the line end) into styles (one per byte, as many as the line has).
	// continued: the previous line ended with a continuation. Returns whether this line does.
	bool StyleLine(std::string_view line, bool continued, std::span<uint8_t> styles);

	// a Scintilla lexer that styles with StyleLine
	Scintilla::ILexer5* Create();
}
