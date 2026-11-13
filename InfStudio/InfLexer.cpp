#include "pch.h"
#include "InfLexer.h"

#include <cassert>
#include <algorithm>
#include <lexilla/lexlib/LexAccessor.h>
#include <lexilla/lexlib/Accessor.h>
#include <lexilla/lexlib/LexerModule.h>

namespace {
	using namespace InfLexer;

	bool IsBlank(char ch) noexcept {
		return ch == ' ' || ch == '\t';
	}

	bool IsDigit(char ch) noexcept {
		return ch >= '0' && ch <= '9';
	}

	bool IsHexDigit(char ch) noexcept {
		return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
	}

	// 123 or 0x1F
	bool IsNumber(std::string_view text) noexcept {
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
			return std::all_of(text.begin() + 2, text.end(), IsHexDigit);
		return !text.empty() && std::all_of(text.begin(), text.end(), IsDigit);
	}

	// the end of the string that starts (with its quote) at start: after its closing quote, or the line's end
	size_t StringEnd(std::string_view line, size_t start) noexcept {
		for (size_t i = start + 1; i < line.size(); i++) {
			if (line[i] == '"') {
				if (i + 1 < line.size() && line[i + 1] == '"') {
					i++;		// "" is a quote in the string
					continue;
				}
				return i + 1;
			}
		}
		return line.size();
	}

	// the '=' between the key and the value: outside quotes, before the comment
	size_t FindAssignment(std::string_view line, size_t from) noexcept {
		for (size_t i = from; i < line.size(); i++) {
			switch (line[i]) {
				case '"': i = StringEnd(line, i) - 1; break;
				case ';': return std::string_view::npos;
				case '=': return i;
			}
		}
		return std::string_view::npos;
	}

	// a '\' with nothing but blanks after it to the end of the line
	bool IsContinuation(std::string_view line, size_t i) noexcept {
		if (line[i] != '\\')
			return false;
		while (++i < line.size())
			if (!IsBlank(line[i]))
				return false;
		return true;
	}

	void Fill(std::span<uint8_t> styles, size_t from, size_t to, Style style) noexcept {
		std::fill(styles.begin() + from, styles.begin() + to, (uint8_t)style);
	}

	// %strkey% at start, or 0 if it isn't one (a lone '%', or %%); within limit
	size_t TokenLength(std::string_view line, size_t start, size_t limit) noexcept {
		for (size_t i = start + 1; i < limit; i++) {
			if (line[i] == '%')
				return i == start + 1 ? 0 : i - start + 1;
			if (line[i] == '"' || line[i] == ';' || line[i] == ',' || IsBlank(line[i]))
				break;
		}
		return 0;
	}

	//
	// [from, to) of the line: strings, string keys, commas, comments, numbers, and the rest in plain.
	// Returns whether the line continues (a '\' at its end).
	//
	bool StyleRange(std::string_view line, size_t from, size_t to, Style plain, std::span<uint8_t> styles) {
		size_t i = from;
		while (i < to) {
			auto ch = line[i];
			if (IsBlank(ch)) {
				i++;
			}
			else if (ch == ';') {
				Fill(styles, i, line.size(), Comment);
				return false;
			}
			else if (ch == '"') {
				auto end = std::min(StringEnd(line, i), to);
				Fill(styles, i, end, String);
				// string keys are replaced inside strings too
				for (size_t k = i + 1; k < end; k++) {
					if (line[k] == '%') {
						if (auto len = TokenLength(line, k, end); len > 0) {
							Fill(styles, k, k + len, Token);
							k += len - 1;
						}
						else if (k + 1 < end && line[k + 1] == '%') {
							k++;
						}
					}
				}
				i = end;
			}
			else if (ch == '%' && TokenLength(line, i, to) > 0) {
				auto len = TokenLength(line, i, to);
				Fill(styles, i, i + len, Token);
				i += len;
			}
			else if (ch == ',' || ch == '=') {
				styles[i++] = Operator;
			}
			else if (to == line.size() && IsContinuation(line, i)) {
				styles[i] = Operator;
				return true;
			}
			else {
				// a word: up to a blank or anything that has a meaning of its own
				auto start = i;
				while (i < to) {
					auto c = line[i];
					if (IsBlank(c) || c == ';' || c == '"' || c == ',' || c == '=')
						break;
					if (c == '%') {
						if (i + 1 < to && line[i + 1] == '%') {
							i += 2;		// %% is a literal percent
							continue;
						}
						if (TokenLength(line, i, to) > 0)
							break;
					}
					if (to == line.size() && IsContinuation(line, i))
						break;
					i++;
				}
				// a number is a whole value: not part of a key, and not a word such as 10.0.1.0
				auto style = plain != Key && IsNumber(line.substr(start, i - start)) ? Number : plain;
				Fill(styles, start, i, style);
			}
		}
		return false;
	}

	void ColouriseInfDoc(Sci_PositionU startPos, Sci_Position length, int /*initStyle*/, Lexilla::WordList*[], Lexilla::Accessor& styler) {
		// whole lines, from the start of the first one (the previous line's state says whether it continues)
		auto line = styler.GetLine(startPos);
		Sci_PositionU pos = styler.LineStart(line);
		const Sci_PositionU end = std::min<Sci_PositionU>(startPos + length, styler.Length());
		styler.StartAt(pos);
		styler.StartSegment(pos);

		bool continued = line > 0 && (styler.GetLineState(line - 1) & 1);
		std::string text;
		std::vector<uint8_t> styles;
		while (pos < end) {
			Sci_PositionU lineEnd = std::min<Sci_PositionU>(styler.LineStart(line + 1), styler.Length());
			if (lineEnd <= pos)
				break;
			text.clear();
			for (auto p = pos; p < lineEnd; p++)
				text.push_back(styler[p]);
			auto content = text.size();
			while (content > 0 && (text[content - 1] == '\r' || text[content - 1] == '\n'))
				content--;

			styles.assign(content, Default);
			auto wasContinued = continued;
			continued = StyleLine(std::string_view(text.data(), content), continued, styles);

			// folding: a section's header line, and the lines after it in it (those before the first section in none)
			auto first = std::string_view(text.data(), content).find_first_not_of(" \t");
			int level;
			if (!wasContinued && first != std::string_view::npos && styles[first] == Section)
				level = SC_FOLDLEVELBASE | SC_FOLDLEVELHEADERFLAG;
			else {
				auto previous = line > 0 ? styler.LevelAt(line - 1) : SC_FOLDLEVELBASE;
				auto inSection = (previous & SC_FOLDLEVELHEADERFLAG) || (previous & SC_FOLDLEVELNUMBERMASK) > SC_FOLDLEVELBASE;
				level = inSection ? SC_FOLDLEVELBASE + 1 : SC_FOLDLEVELBASE;
			}
			styler.SetLevel(line, level);
			for (size_t k = 0; k < content; k++)
				if (k + 1 == content || styles[k + 1] != styles[k])
					styler.ColourTo(pos + k, styles[k]);
			if (content < text.size())
				styler.ColourTo(lineEnd - 1, Default);
			styler.SetLineState(line, continued ? 1 : 0);

			pos = lineEnd;
			line++;
		}
		styler.Flush();
	}

	const Lexilla::LexerModule lmInf(SCLEX_AUTOMATIC, ColouriseInfDoc, "inf");
}

bool InfLexer::StyleLine(std::string_view line, bool continued, std::span<uint8_t> styles) {
	assert(styles.size() >= line.size());
	Fill(styles, 0, line.size(), Default);

	size_t i = 0;
	while (i < line.size() && IsBlank(line[i]))
		i++;
	if (i == line.size())
		return false;

	if (!continued && line[i] == '[') {
		// [section name], then perhaps a comment
		auto end = i + 1;
		while (end < line.size() && line[end] != ']' && line[end] != ';')
			end++;
		if (end < line.size() && line[end] == ']')
			end++;
		Fill(styles, i, end, Section);
		StyleRange(line, end, line.size(), Default, styles);
		return false;
	}

	if (!continued) {
		if (auto eq = FindAssignment(line, i); eq != std::string_view::npos) {
			StyleRange(line, i, eq, Key, styles);
			styles[eq] = Operator;
			i = eq + 1;
		}
	}
	return StyleRange(line, i, line.size(), Default, styles);
}

Scintilla::ILexer5* InfLexer::Create() {
	return lmInf.Create();
}
