#include "pch.h"
#include "InfLines.h"
#include "InfLexer.h"

#include <algorithm>

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

namespace {
	bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
		return text.size() >= prefix.size() && InfLines::EqualNoCase(text.substr(0, prefix.size()), prefix);
	}

	bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
		return text.size() >= suffix.size() && InfLines::EqualNoCase(text.substr(text.size() - suffix.size()), suffix);
	}

	bool In(InfLines::Names const& names, std::string_view name) {
		return std::any_of(names.begin(), names.end(), [&](auto n) { return InfLines::EqualNoCase(n, name); });
	}
}

const InfLines::Names InfLines::SectionValueKeys = { "CopyFiles", "DelFiles", "RenFiles", "AddReg", "DelReg", "BitReg", "AddProperty",
	"DelProperty", "ProfileItems" };
const InfLines::Names InfLines::FileSectionKeys = { "CopyFiles", "DelFiles", "RenFiles" };
const InfLines::Names InfLines::StandardSections = { "Version", "Manufacturer", "Strings", "SourceDisksNames", "SourceDisksFiles", "DestinationDirs" };

InfLines::InfModel::InfModel(std::string_view text) : Lines(StyleLines(text)) {
	std::string_view section;
	std::vector<size_t> modelLines;		// the lines of [Manufacturer]
	for (size_t index = 0; index < Lines.size(); index++) {
		auto const& line = Lines[index];
		if (auto name = SectionName(line)) {
			section = *name;
			Defined.push_back(section);
			SectionOfLine.push_back(section);
			continue;
		}
		SectionOfLine.push_back(section);
		auto key = KeyOf(line);
		auto fields = ValueFields(line);
		auto reference = [&](Field const& f, ReferenceKind kind) {
			References.push_back({ f.Text, line.Start + f.Start, line.Start + f.End, kind, key });
		};
		if (EqualNoCase(section, "Strings") || StartsWithNoCase(section, "Strings.")) {
			if (!key.empty())
				StringKeys.push_back(key);
		}
		else if (EqualNoCase(section, "SourceDisksFiles") || StartsWithNoCase(section, "SourceDisksFiles.")) {
			if (!key.empty())
				SourceFiles.push_back(key);
		}
		else if (EqualNoCase(section, "Manufacturer")) {
			if (fields.empty() || fields[0].Text.empty())
				continue;
			if (fields.size() == 1) {
				reference(fields[0], ReferenceKind::Models);
				continue;
			}
			// decorated: a models section for each decoration (named as the text doesn't have it: kept here)
			for (size_t i = 1; i < fields.size(); i++) {
				if (fields[i].Text.empty())
					continue;
				auto& name = m_Composed.emplace_back(std::make_unique<std::string>(std::string(fields[0].Text) + "." + std::string(fields[i].Text)));
				References.push_back({ *name, line.Start + fields[0].Start, line.Start + fields[i].End, ReferenceKind::Models, key });
			}
		}
		else if (In(SectionValueKeys, key)) {
			for (auto const& f : fields)
				if (!f.Text.empty() && f.Text[0] != '@')
					reference(f, ReferenceKind::List);
		}
		else if (EqualNoCase(key, "AddService")) {
			if (fields.size() > 2 && !fields[2].Text.empty())
				reference(fields[2], ReferenceKind::Service);
		}
		else if (EqualNoCase(key, "KmdfService") || EqualNoCase(key, "UmdfService")) {
			if (fields.size() > 1 && !fields[1].Text.empty())
				reference(fields[1], ReferenceKind::WdfService);
		}
	}
	// the install sections: the first field of the device lines of the models sections
	auto models = Named(ReferenceKind::Models);
	for (size_t i = 0; i < Lines.size(); i++) {
		auto const& line = Lines[i];
		if (SectionOfLine[i].empty() || !In(models, SectionOfLine[i]) || SectionName(line) || KeyOf(line).empty())
			continue;
		auto fields = ValueFields(line);
		if (!fields.empty() && !fields[0].Text.empty())
			References.push_back({ fields[0].Text, line.Start + fields[0].Start, line.Start + fields[0].End, ReferenceKind::Install, KeyOf(line) });
	}
}

InfLines::Names InfLines::InfModel::Named(ReferenceKind kind, Names const* directives) const {
	Names names;
	for (auto const& r : References)
		if (r.Kind == kind && (!directives || In(*directives, r.Directive)))
			names.push_back(r.Name);
	return names;
}

bool InfLines::InfModel::IsDefined(std::string_view name) const {
	return In(Defined, name);
}

bool InfLines::InfModel::IsInstallDefined(std::string_view name) const {
	return std::any_of(Defined.begin(), Defined.end(), [&](auto d) {
		return EqualNoCase(d, name) || (StartsWithNoCase(d, name) && d.size() > name.size() && d[name.size()] == '.');
	});
}

InfLines::SectionKind InfLines::InfModel::KindOf(std::string_view section) const {
	if (EqualNoCase(section, "Version"))
		return SectionKind::Version;
	if (EqualNoCase(section, "DestinationDirs"))
		return SectionKind::DestinationDirs;
	if (EndsWithNoCase(section, ".Services"))
		return SectionKind::Services;
	if (EndsWithNoCase(section, ".HW"))
		return SectionKind::Hw;
	if (EndsWithNoCase(section, ".Wdf"))
		return SectionKind::Wdf;
	if (EndsWithNoCase(section, ".CoInstallers"))
		return SectionKind::CoInstallers;
	if (EndsWithNoCase(section, ".Interfaces"))
		return SectionKind::Interfaces;
	if (In(Named(ReferenceKind::Service), section))
		return SectionKind::Service;
	if (In(Named(ReferenceKind::WdfService), section))
		return SectionKind::WdfService;
	if (StartsWithNoCase(section, "DefaultInstall"))
		return SectionKind::Install;
	for (auto install : Named(ReferenceKind::Install))
		if (EqualNoCase(section, install) || (StartsWithNoCase(section, install) && section.size() > install.size() && section[install.size()] == '.'))
			return SectionKind::Install;
	return SectionKind::None;
}

bool InfLines::InfModel::IsListSection(std::string_view section) const {
	return KindOf(section) == SectionKind::None && !In(StandardSections, section) && !In(Named(ReferenceKind::Models), section)
		&& !StartsWithNoCase(section, "Strings.") && !StartsWithNoCase(section, "SourceDisks") && !EqualNoCase(section, "ControlFlags");
}
