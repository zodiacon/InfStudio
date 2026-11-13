#include "pch.h"
#include "InfChecks.h"
#include "InfLines.h"
#include "InfLexer.h"

#include <algorithm>
#include <format>

using namespace InfLines;

namespace {
	bool In(Names const& names, std::string_view name) {
		return std::any_of(names.begin(), names.end(), [&](auto n) { return EqualNoCase(n, name); });
	}

	bool IsStrings(std::string_view section) {
		return EqualNoCase(section, "Strings") || (section.size() > 8 && _strnicmp(section.data(), "Strings.", 8) == 0);
	}

	bool AllDigits(std::string_view text) {
		return !text.empty() && std::all_of(text.begin(), text.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
	}

	const Names CopyFilesKey = { "CopyFiles" };
}

std::vector<InfProblem> InfChecks::Check(std::string_view text) {
	InfModel model(text);
	std::vector<InfProblem> problems;

	// string keys
	for (size_t index = 0; index < model.Lines.size(); index++) {
		auto const& line = model.Lines[index];
		if (IsStrings(model.SectionOfLine[index]))
			continue;
		for (size_t i = 0; i < line.Text.size(); i++) {
			if (line.Styles[i] != InfLexer::Token || line.Text[i] != '%')
				continue;
			auto close = line.Text.find('%', i + 1);
			if (close == std::string_view::npos)
				break;
			auto name = line.Text.substr(i + 1, close - i - 1);
			if (!AllDigits(name) && !In(model.StringKeys, name))
				problems.push_back({ line.Start + i, line.Start + close + 1, true, std::format("%{}% isn't defined in [Strings]", name) });
			i = close;
		}
	}

	// sections
	for (auto const& r : model.References) {
		switch (r.Kind) {
			case ReferenceKind::Models:
				if (!model.IsDefined(r.Name))
					problems.push_back({ r.Start, r.End, true, std::format("The models section [{}] isn't in the file", r.Name) });
				break;
			case ReferenceKind::Install:
				if (!model.IsInstallDefined(r.Name))
					problems.push_back({ r.Start, r.End, true, std::format("The install section [{0}] (or a decorated one: [{0}.NT]...) isn't in the file", r.Name) });
				break;
			default:
				if (!model.IsDefined(r.Name))
					problems.push_back({ r.Start, r.End, false, std::format("[{}] isn't in the file (an included INF may have it)", r.Name) });
				break;
		}
	}

	// files to copy: the source of each line of the sections CopyFiles names (its second field, else its first)
	auto report = [&](std::string_view file, size_t start, size_t end) {
		// (a name a string key gives, %name%, isn't known here)
		if (!file.empty() && file.find('%') == std::string_view::npos && !In(model.SourceFiles, file))
			problems.push_back({ start, end, true, std::format("{} isn't in [SourceDisksFiles]", file) });
	};
	auto copied = model.Named(ReferenceKind::List, &CopyFilesKey);
	for (size_t index = 0; index < model.Lines.size(); index++) {
		auto const& line = model.Lines[index];
		auto section = model.SectionOfLine[index];
		if (section.empty() || SectionName(line))
			continue;
		if (In(copied, section)) {
			auto fields = ValueFields(line);
			if (fields.empty() || fields[0].Text.empty())
				continue;
			auto const& source = fields.size() > 1 && !fields[1].Text.empty() ? fields[1] : fields[0];
			report(source.Text, line.Start + fields[0].Start, line.Start + source.End);
		}
		// CopyFiles = @file: the file itself
		if (EqualNoCase(KeyOf(line), "CopyFiles"))
			for (auto const& f : ValueFields(line))
				if (f.Text.size() > 1 && f.Text[0] == '@')
					report(f.Text.substr(1), line.Start + f.Start, line.Start + f.End);
	}

	std::sort(problems.begin(), problems.end(), [](auto const& a, auto const& b) { return a.Start < b.Start; });
	return problems;
}
