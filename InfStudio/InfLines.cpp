#include "pch.h"
#include "InfLines.h"
#include "InfLexer.h"

using namespace InfLexer;

std::vector<InfLines::StyledLine> InfLines::StyleLines(std::string_view text) {
	std::vector<StyledLine> lines;
	bool continued = false;
	for (size_t start = 0; start <= text.size();) {
		auto end = text.find('\n', start);
		auto next = end == std::string_view::npos ? text.size() + 1 : end + 1;
		if (end == std::string_view::npos)
			end = text.size();
		auto content = end;
		if (content > start && text[content - 1] == '\r')
			content--;
		StyledLine line{ start, text.substr(start, content - start) };
		line.Styles.resize(line.Text.size());
		continued = StyleLine(line.Text, continued, line.Styles);
		lines.push_back(std::move(line));
		start = next;
	}
	return lines;
}

InfLines::StyledLine const* InfLines::LineAt(std::vector<StyledLine> const& lines, size_t pos) {
	for (auto const& line : lines)
		if (pos >= line.Start && pos <= line.Start + line.Text.size())
			return &line;
	return nullptr;
}

std::string_view InfLines::Trim(std::string_view text) {
	auto first = text.find_first_not_of(" \t");
	if (first == std::string_view::npos)
		return {};
	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool InfLines::EqualNoCase(std::string_view a, std::string_view b) {
	return a.size() == b.size() && _strnicmp(a.data(), b.data(), a.size()) == 0;
}

std::optional<std::string_view> InfLines::SectionName(StyledLine const& line) {
	auto open = line.Text.find('[');
	if (open == std::string_view::npos || line.Styles[open] != Section)
		return {};
	auto close = line.Text.find(']', open);
	if (close == std::string_view::npos)
		close = line.Text.size();
	return Trim(line.Text.substr(open + 1, close - open - 1));
}

std::string_view InfLines::KeyOf(StyledLine const& line) {
	for (size_t i = 0; i < line.Text.size(); i++)
		if (line.Styles[i] == Operator && line.Text[i] == '=')
			return Trim(line.Text.substr(0, i));
	return {};
}

std::vector<InfLines::Field> InfLines::ValueFields(StyledLine const& line) {
	size_t start = 0;
	for (size_t i = 0; i < line.Text.size(); i++)
		if (line.Styles[i] == Operator && line.Text[i] == '=') {
			start = i + 1;
			break;
		}
	std::vector<Field> fields;
	auto add = [&](size_t from, size_t to) {
		auto text = line.Text.substr(from, to - from);
		auto trimmed = Trim(text);
		auto offset = trimmed.empty() ? to : from + (trimmed.data() - text.data());
		fields.push_back({ offset, offset + trimmed.size(), trimmed });
	};
	size_t i = start;
	for (; i < line.Text.size() && line.Styles[i] != Comment; i++) {
		if (line.Styles[i] == Operator && line.Text[i] == ',') {
			add(start, i);
			start = i + 1;
		}
	}
	add(start, i);
	return fields;
}
