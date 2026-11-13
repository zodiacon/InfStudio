#include "pch.h"
#include "InfCompletion.h"
#include "InfLines.h"
#include "InfLexer.h"

#include <algorithm>
#include <set>

using namespace InfLines;

namespace {
	using Names = std::vector<std::string_view>;

	const Names VersionKeys = { "Signature", "Class", "ClassGuid", "ClassVer", "Provider", "ExtensionId", "CatalogFile",
		"DriverVer", "PnpLockdown", "DriverPackageDisplayName", "DriverPackageType" };
	const Names InstallKeys = { "AddComponent", "AddEventProvider", "AddPowerSetting", "AddProperty", "AddReg", "AddSoftware",
		"BitReg", "BusType", "Characteristics", "CopyFiles", "CopyINF", "DelFiles", "DelProperty", "DelReg", "DriverVer",
		"FeatureScore", "Include", "Ini2Reg", "LogConfig", "Needs", "NetCfgInstanceId", "OptionDesc", "ProfileItems", "Reboot",
		"RenFiles", "UpdateIniFields", "UpdateInis" };
	const Names HwKeys = { "AddProperty", "AddReg", "BitReg", "DelProperty", "DelReg", "Include", "Needs" };
	const Names ServicesKeys = { "AddService", "DelService", "Include", "Needs" };
	const Names WdfKeys = { "KmdfService", "UmdfService", "UmdfServiceOrder", "UmdfKernelModeClientPolicy",
		"UmdfMethodNeitherAction", "UmdfDispatcher", "UmdfImpersonationLevel", "UmdfFileObjectPolicy", "UmdfFsContextUsePolicy",
		"UmdfHostProcessSharing", "UmdfDirectHardwareAccess", "UmdfRegisterAccessMode" };
	const Names ServiceKeys = { "DisplayName", "Description", "ServiceType", "StartType", "ErrorControl", "ServiceBinary",
		"LoadOrderGroup", "Dependencies", "StartName", "AddReg", "DelReg", "BitReg", "Security", "AddTrigger", "DelayedAutoStart" };
	const Names WdfServiceKeys = { "KmdfLibraryVersion", "UmdfLibraryVersion", "ServiceBinary", "UmdfExtensions" };
	const Names CoInstallersKeys = { "AddReg", "CopyFiles" };
	const Names InterfacesKeys = { "AddInterface" };

	// the directives whose every field names a section
	const Names SectionValueKeys = { "CopyFiles", "DelFiles", "RenFiles", "AddReg", "DelReg", "BitReg", "AddProperty",
		"DelProperty", "Needs", "ProfileItems" };
	const Names FileSectionKeys = { "CopyFiles", "DelFiles", "RenFiles" };
	const Names StandardSections = { "Version", "Manufacturer", "Strings", "SourceDisksNames", "SourceDisksFiles", "DestinationDirs" };

	bool In(Names const& names, std::string_view name) {
		return std::any_of(names.begin(), names.end(), [&](auto n) { return EqualNoCase(n, name); });
	}

	bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
		return text.size() >= prefix.size() && EqualNoCase(text.substr(0, prefix.size()), prefix);
	}

	bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
		return text.size() >= suffix.size() && EqualNoCase(text.substr(text.size() - suffix.size()), suffix);
	}

	bool IsWordChar(char ch) {
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-' || ch == '$';
	}

	enum class Kind { None, Version, DestinationDirs, Install, Hw, Services, Wdf, Service, WdfService, CoInstallers, Interfaces };

	// what the file has, and what its directives name
	struct Model {
		std::vector<StyledLine> Lines;
		std::vector<std::string_view> SectionOfLine;	// the section each line is in (empty before the first)
		Names Defined;					// the sections, in the file's order
		Names Models, Installs, Services, WdfServices, Lists, FileSections;	// named by directives
		Names StringKeys;

		explicit Model(std::string_view text) : Lines(StyleLines(text)) {
			std::string_view section;
			for (auto const& line : Lines) {
				if (auto name = SectionName(line)) {
					section = *name;
					Defined.push_back(section);
					SectionOfLine.push_back(section);
					continue;
				}
				SectionOfLine.push_back(section);
				auto key = KeyOf(line);
				auto fields = ValueFields(line);
				auto field = [&](size_t i) { return i < fields.size() ? fields[i].Text : std::string_view{}; };
				if (EqualNoCase(section, "Strings") || StartsWithNoCase(section, "Strings.")) {
					if (!key.empty())
						StringKeys.push_back(key);
				}
				else if (EqualNoCase(section, "Manufacturer")) {
					if (fields.empty() || fields[0].Text.empty())
						continue;
					if (fields.size() == 1)
						Models.push_back(fields[0].Text);
					// (a decorated models section: the name and the decoration; kept as the line has them, joined when compared)
					for (size_t i = 1; i < fields.size(); i++)
						if (!fields[i].Text.empty())
							m_Decorated.push_back(std::string(fields[0].Text) + "." + std::string(fields[i].Text));
				}
				else if (In(SectionValueKeys, key)) {
					for (auto const& f : fields)
						if (!f.Text.empty() && f.Text[0] != '@') {
							Lists.push_back(f.Text);
							if (In(FileSectionKeys, key))
								FileSections.push_back(f.Text);
						}
				}
				else if (EqualNoCase(key, "AddService")) {
					if (!field(2).empty())
						Services.push_back(field(2));
				}
				else if (EqualNoCase(key, "KmdfService") || EqualNoCase(key, "UmdfService")) {
					if (!field(1).empty())
						WdfServices.push_back(field(1));
				}
			}
			for (auto const& name : m_Decorated)
				Models.push_back(name);
			// the install sections: the first field of the lines of the models sections
			for (size_t i = 0; i < Lines.size(); i++)
				if (!SectionOfLine[i].empty() && In(Models, SectionOfLine[i]) && !SectionName(Lines[i]) && !KeyOf(Lines[i]).empty()) {
					auto fields = ValueFields(Lines[i]);
					if (!fields.empty() && !fields[0].Text.empty())
						Installs.push_back(fields[0].Text);
				}
		}

		bool IsDefined(std::string_view name) const {
			return In(Defined, name);
		}
		// an install section is there if it is, or one of its decorated forms is
		bool IsInstallDefined(std::string_view name) const {
			return std::any_of(Defined.begin(), Defined.end(), [&](auto d) {
				return EqualNoCase(d, name) || (StartsWithNoCase(d, name) && d.size() > name.size() && d[name.size()] == '.');
			});
		}

		Kind KindOf(std::string_view section) const {
			if (EqualNoCase(section, "Version"))
				return Kind::Version;
			if (EqualNoCase(section, "DestinationDirs"))
				return Kind::DestinationDirs;
			if (EndsWithNoCase(section, ".Services"))
				return Kind::Services;
			if (EndsWithNoCase(section, ".HW"))
				return Kind::Hw;
			if (EndsWithNoCase(section, ".Wdf"))
				return Kind::Wdf;
			if (EndsWithNoCase(section, ".CoInstallers"))
				return Kind::CoInstallers;
			if (EndsWithNoCase(section, ".Interfaces"))
				return Kind::Interfaces;
			if (In(Services, section))
				return Kind::Service;
			if (In(WdfServices, section))
				return Kind::WdfService;
			if (StartsWithNoCase(section, "DefaultInstall"))
				return Kind::Install;
			for (auto install : Installs)
				if (EqualNoCase(section, install) || (StartsWithNoCase(section, install) && section.size() > install.size() && section[install.size()] == '.'))
					return Kind::Install;
			return Kind::None;
		}

		// a section a directive's value can name: a list (files, registry...), not one with keys or a well-known one
		bool IsListSection(std::string_view section) const {
			return KindOf(section) == Kind::None && !In(StandardSections, section) && !In(Models, section)
				&& !StartsWithNoCase(section, "Strings.") && !StartsWithNoCase(section, "SourceDisks") && !EqualNoCase(section, "ControlFlags");
		}

	private:
		std::vector<std::string> m_Decorated;
	};

	// each once (whatever its case), sorted
	std::vector<std::string> Unique(Names const& names) {
		std::vector<std::string> items;
		for (auto name : names)
			if (!name.empty() && std::none_of(items.begin(), items.end(), [&](auto const& i) { return EqualNoCase(i, name); }))
				items.emplace_back(name);
		std::sort(items.begin(), items.end(), [](auto const& a, auto const& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
		return items;
	}

	Names const* KeysOf(Kind kind) {
		switch (kind) {
			case Kind::Version: return &VersionKeys;
			case Kind::Install: return &InstallKeys;
			case Kind::Hw: return &HwKeys;
			case Kind::Services: return &ServicesKeys;
			case Kind::Wdf: return &WdfKeys;
			case Kind::Service: return &ServiceKeys;
			case Kind::WdfService: return &WdfServiceKeys;
			case Kind::CoInstallers: return &CoInstallersKeys;
			case Kind::Interfaces: return &InterfacesKeys;
		}
		return nullptr;
	}
}

std::optional<InfCompletion::List> InfCompletion::Complete(std::string_view text, size_t caret) {
	Model model(text);
	auto line = LineAt(model.Lines, caret);
	if (!line)
		return {};
	auto lineIndex = line - model.Lines.data();
	auto offset = caret - line->Start;
	auto const& styles = line->Styles;
	// nothing in a comment
	for (size_t i = 0; i < offset; i++)
		if (styles[i] == InfLexer::Comment)
			return {};

	auto wordStart = offset;
	while (wordStart > 0 && IsWordChar(line->Text[wordStart - 1]))
		wordStart--;
	auto rest = line->Text.substr(offset);
	auto before = Trim(line->Text.substr(0, wordStart));

	// a string key: after a '%' that opens one (the percents before it are in pairs: whole keys, or %% for a percent)
	if (wordStart > 0 && line->Text[wordStart - 1] == '%') {
		auto percents = std::count(line->Text.begin(), line->Text.begin() + (wordStart - 1), '%');
		if (percents % 2 != 0)
			return {};
		return List{ line->Start + wordStart, Unique(model.StringKeys), rest.starts_with('%') ? "" : "%" };
	}

	// a section header
	if (before == "[") {
		Names names(StandardSections.begin(), StandardSections.end());
		for (auto list : { &model.Models, &model.Services, &model.WdfServices, &model.Lists })
			names.insert(names.end(), list->begin(), list->end());
		Names missing;
		for (auto name : names)
			if (!model.IsDefined(name))
				missing.push_back(name);
		for (auto install : model.Installs)
			if (!model.IsInstallDefined(install))
				missing.push_back(install);
		return List{ line->Start + wordStart, Unique(missing), rest.find(']') == std::string_view::npos ? "]" : "" };
	}

	auto section = model.SectionOfLine[lineIndex];
	if (section.empty() || SectionName(*line))
		return {};

	size_t eq = std::string_view::npos;
	for (size_t i = 0; i < line->Text.size(); i++)
		if (styles[i] == InfLexer::Operator && line->Text[i] == '=') {
			eq = i;
			break;
		}

	// a key, at the line's start
	if (eq == std::string_view::npos || offset <= eq) {
		if (!before.empty())
			return {};
		auto kind = model.KindOf(section);
		auto suffix = eq == std::string_view::npos ? " = " : "";
		if (kind == Kind::DestinationDirs) {
			// DefaultDestDir, and the file sections without a line here yet
			Names names{ "DefaultDestDir" };
			for (auto files : model.FileSections) {
				bool listed = false;
				for (size_t i = 0; i < model.Lines.size(); i++)
					if (i != (size_t)lineIndex && EqualNoCase(model.SectionOfLine[i], section) && EqualNoCase(KeyOf(model.Lines[i]), files))
						listed = true;
				if (!listed)
					names.push_back(files);
			}
			return List{ line->Start + wordStart, Unique(names), suffix };
		}
		auto keys = KeysOf(kind);
		if (!keys)
			return {};
		return List{ line->Start + wordStart, Unique(*keys), suffix };
	}

	// a field of a value that names sections
	auto key = KeyOf(*line);
	size_t field = 0, fieldStart = eq + 1;
	for (size_t i = eq + 1; i < wordStart; i++)
		if (styles[i] == InfLexer::Operator && line->Text[i] == ',') {
			field++;
			fieldStart = i + 1;
		}
	if (!Trim(line->Text.substr(fieldStart, wordStart - fieldStart)).empty())
		return {};
	bool names = In(SectionValueKeys, key) || (EqualNoCase(key, "AddService") && field == 2)
		|| ((EqualNoCase(key, "KmdfService") || EqualNoCase(key, "UmdfService")) && field == 1);
	if (!names)
		return {};
	Names sections;
	for (auto defined : model.Defined)
		if (!EqualNoCase(defined, section) && model.IsListSection(defined))
			sections.push_back(defined);
	return List{ line->Start + wordStart, Unique(sections), "" };
}
