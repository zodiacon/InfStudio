#include "pch.h"
#include "InfCompletion.h"
#include "InfLines.h"
#include "InfLexer.h"

#include <algorithm>

using namespace InfLines;

namespace {
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

	bool In(Names const& names, std::string_view name) {
		return std::any_of(names.begin(), names.end(), [&](auto n) { return EqualNoCase(n, name); });
	}

	bool IsWordChar(char ch) {
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-' || ch == '$';
	}

	// each once (whatever its case), sorted
	std::vector<std::string> Unique(Names const& names) {
		std::vector<std::string> items;
		for (auto name : names)
			if (!name.empty() && std::none_of(items.begin(), items.end(), [&](auto const& i) { return EqualNoCase(i, name); }))
				items.emplace_back(name);
		std::sort(items.begin(), items.end(), [](auto const& a, auto const& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
		return items;
	}

	Names const* KeysOf(SectionKind kind) {
		switch (kind) {
			case SectionKind::Version: return &VersionKeys;
			case SectionKind::Install: return &InstallKeys;
			case SectionKind::Hw: return &HwKeys;
			case SectionKind::Services: return &ServicesKeys;
			case SectionKind::Wdf: return &WdfKeys;
			case SectionKind::Service: return &ServiceKeys;
			case SectionKind::WdfService: return &WdfServiceKeys;
			case SectionKind::CoInstallers: return &CoInstallersKeys;
			case SectionKind::Interfaces: return &InterfacesKeys;
		}
		return nullptr;
	}
}

std::optional<InfCompletion::List> InfCompletion::Complete(std::string_view text, size_t caret, Context const& context) {
	InfModel model(text);
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
		Names missing;
		for (auto name : StandardSections)
			if (!model.IsDefined(name))
				missing.push_back(name);
		for (auto const& r : model.References) {
			bool there = r.Kind == ReferenceKind::Install ? model.IsInstallDefined(r.Name) : model.IsDefined(r.Name);
			if (!there)
				missing.push_back(r.Name);
		}
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
		if (kind == SectionKind::DestinationDirs) {
			// DefaultDestDir, and the file sections without a line here yet
			Names names{ "DefaultDestDir" };
			for (auto files : model.Named(ReferenceKind::List, &FileSectionKeys)) {
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
	// other INFs, and their sections
	if (EqualNoCase(key, "Include"))
		return List{ line->Start + wordStart, Unique(Names(context.InfNames.begin(), context.InfNames.end())), "" };
	if (EqualNoCase(key, "Needs")) {
		Names sections;
		if (context.Included)
			for (auto file : IncludedFiles(model.Lines, section))
				if (auto other = context.Included(file)) {
					// (the names are in the other's text, which stays: not in its model)
					InfModel included(*other);
					for (auto name : included.Defined) {
						// not [Version], [Strings], [SourceDisksNames.amd64]...: what every INF has, not to be needed
						bool standard = std::any_of(StandardSections.begin(), StandardSections.end(), [&](auto s) {
							return EqualNoCase(name, s) || (name.size() > s.size() && name[s.size()] == '.' && EqualNoCase(name.substr(0, s.size()), s));
						});
						if (!standard)
							sections.push_back(name);
					}
				}
		return List{ line->Start + wordStart, Unique(sections), "" };
	}
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
