#include "pch.h"
#include "InfNavigation.h"
#include "InfLexer.h"
#include "InfLines.h"

namespace {
	using namespace InfLexer;
	using namespace InfLines;

	// the line of a string key: in [Strings], else in a localized [Strings.xxxx]
	StyledLine const* FindStringKey(std::vector<StyledLine> const& lines, std::string_view name) {
		StyledLine const* localized = nullptr;
		bool inStrings = false, inLocalized = false;
		for (auto const& line : lines) {
			if (auto section = SectionName(line)) {
				inStrings = EqualNoCase(*section, "Strings");
				inLocalized = !inStrings && section->size() > 8 && _strnicmp(section->data(), "Strings.", 8) == 0;
				continue;
			}
			if (!inStrings && !(inLocalized && !localized))
				continue;
			// the key: what's styled as one, from the line's start
			size_t i = 0;
			while (i < line.Text.size() && line.Styles[i] != Key && line.Styles[i] != Operator)
				i++;
			auto start = i;
			while (i < line.Text.size() && line.Styles[i] == Key)
				i++;
			if (i > start && EqualNoCase(Trim(line.Text.substr(start, i - start)), name)) {
				if (inStrings)
					return &line;
				localized = &line;
			}
		}
		return localized;
	}

	// the string key at offset in its line (between its percents), if there's one
	std::optional<std::string_view> StringKeyAt(StyledLine const& line, size_t offset) {
		for (size_t i = 0; i < line.Text.size(); i++) {
			if (line.Styles[i] != Token || line.Text[i] != '%')
				continue;
			// a token: from this percent to the next
			auto close = line.Text.find('%', i + 1);
			if (close == std::string_view::npos)
				return {};
			if (offset >= i && offset <= close)
				return line.Text.substr(i + 1, close - i - 1);
			i = close;
		}
		return {};
	}

	// the field around offset in a key or a value (between commas, '=' and the line's ends), if it can name a section
	std::optional<std::string_view> FieldAt(StyledLine const& line, size_t offset) {
		if (offset >= line.Text.size() || (line.Styles[offset] != Default && line.Styles[offset] != Key))
			return {};
		auto separator = [&](size_t i) {
			return line.Styles[i] == Operator || line.Styles[i] == Comment || line.Styles[i] == String || line.Styles[i] == Token;
		};
		auto start = offset, end = offset;
		while (start > 0 && !separator(start - 1))
			start--;
		while (end < line.Text.size() && !separator(end))
			end++;
		auto field = Trim(line.Text.substr(start, end - start));
		if (field.empty())
			return {};
		return field;
	}

	// what StringValueAt gives of a value: a string's text, or the value as it is
	std::string ValueText(StyledLine const& line) {
		auto eq = std::string_view::npos;
		for (size_t i = 0; i < line.Text.size(); i++)
			if (line.Styles[i] == Operator && line.Text[i] == '=') {
				eq = i;
				break;
			}
		if (eq == std::string_view::npos)
			return {};
		auto end = line.Text.size();
		for (size_t i = eq + 1; i < line.Text.size(); i++)
			if (line.Styles[i] == Comment) {
				end = i;
				break;
			}
		auto value = Trim(line.Text.substr(eq + 1, end - eq - 1));
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
			std::string text;
			for (size_t i = 1; i + 1 < value.size(); i++) {
				text += value[i];
				if (value[i] == '"' && value[i + 1] == '"')
					i++;
			}
			return text;
		}
		return std::string(value);
	}
}

size_t InfNavigation::FindDefinition(std::string_view text, size_t pos) {
	auto lines = StyleLines(text);
	auto line = LineAt(lines, pos);
	if (!line)
		return std::string_view::npos;
	auto offset = pos - line->Start;

	if (auto key = StringKeyAt(*line, offset)) {
		auto definition = FindStringKey(lines, *key);
		return definition ? definition->Start : std::string_view::npos;
	}
	if (auto field = FieldAt(*line, offset)) {
		for (auto const& other : lines)
			if (auto section = SectionName(other); section && &other != line && EqualNoCase(*section, *field))
				return other.Start + other.Text.find('[');
	}
	return std::string_view::npos;
}

size_t InfNavigation::FindSection(std::string_view text, std::string_view name) {
	for (auto const& line : StyleLines(text))
		if (auto section = SectionName(line); section && EqualNoCase(*section, name))
			return line.Start + line.Text.find('[');
	return std::string_view::npos;
}

std::optional<InfNavigation::IncludedTarget> InfNavigation::FindIncluded(std::string_view text, size_t pos, IncludedText const& included) {
	auto lines = StyleLines(text);
	auto line = LineAt(lines, pos);
	if (!line)
		return {};
	auto key = KeyOf(*line);
	bool include = EqualNoCase(key, "Include"), needs = EqualNoCase(key, "Needs");
	if (!include && !needs)
		return {};
	// in the value: after the '='
	auto offset = pos - line->Start;
	size_t eq = std::string_view::npos;
	for (size_t i = 0; i < line->Text.size(); i++)
		if (line->Styles[i] == Operator && line->Text[i] == '=') {
			eq = i;
			break;
		}
	if (eq == std::string_view::npos || offset <= eq)
		return {};
	auto field = FieldAt(*line, offset);
	if (!field)
		return {};
	if (include)
		return IncludedTarget{ std::string(*field), {}, included && included(*field) != nullptr };

	// the section the line is in
	std::string_view section;
	for (auto i = line - lines.data(); i >= 0; i--)
		if (auto name = SectionName(lines[i])) {
			section = *name;
			break;
		}
	if (included)
		for (auto file : IncludedFiles(lines, section))
			if (auto other = included(file); other && FindSection(*other, *field) != std::string_view::npos)
				return IncludedTarget{ std::string(file), std::string(*field), true };
	return IncludedTarget{ {}, std::string(*field), false };
}

std::optional<std::string> InfNavigation::StringValueAt(std::string_view text, size_t pos) {
	auto lines = StyleLines(text);
	auto line = LineAt(lines, pos);
	if (!line)
		return {};
	auto key = StringKeyAt(*line, pos - line->Start);
	if (!key)
		return {};
	auto definition = FindStringKey(lines, *key);
	if (!definition)
		return {};
	return ValueText(*definition);
}
