#pragma once

#include <string>
#include <span>
#include <optional>

//
// The INFs File > New from Template makes (through the template wizard, CTemplateDlg). A template's text has placeholders
// that Generate fills in:
//	@DATE@ (DriverVer's date) and @GUID@ (a new GUID, for what must be unique to the package: a device interface,
//	a NetCfgInstanceId, an ExtensionId);
//	what the wizard asks for: @NAME@ (the driver's name: its services, files, catalog), @MFG@ (the provider and
//	manufacturer), @DESC@ (the device's or the service's description), @HWID@ (the hardware ID a device template
//	installs on) and @VERSION@ (DriverVer's version).
// @MFG@ and @DESC@ are in quoted strings in [Strings] ("@MFG@"): Generate escapes a " in them ("").
//
struct InfTemplate {
	PCWSTR Name;		// in the menu
	int Group;			// the menu has a separator between groups
	const char* Text;
	// the defaults of what the wizard asks for
	PCWSTR DriverName;
	PCWSTR Description;
	PCWSTR HardwareId;	// nullptr: the template has none (not a device's)
};

namespace InfTemplates {
	std::span<InfTemplate const> All();

	constexpr wchar_t DefaultManufacturer[] = L"Contoso";
	constexpr wchar_t DefaultVersion[] = L"1.0.0.0";

	struct Values {
		SYSTEMTIME Date;
		GUID Guid;
		// empty: the template's default
		std::wstring DriverName, Manufacturer, Description, HardwareId, Version;
	};
	// today, and a new GUID (the rest the templates' defaults)
	Values NewValues();
	// NewValues with the template's defaults filled in (what the wizard starts with)
	Values DefaultValues(InfTemplate const& infTemplate);
	// the template's text with its placeholders filled in, with CRLF line ends; UTF-8
	std::string Generate(InfTemplate const& infTemplate, Values const& values);
	// what Save As suggests: the driver's name and .inf
	std::wstring FileName(InfTemplate const& infTemplate, Values const& values);
	// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}, upper case
	std::string GuidText(GUID const& guid);

	// what's wrong with the values (the first problem), if anything
	enum class Field { DriverName, Manufacturer, Description, HardwareId, Version };
	struct Problem {
		Field Field;
		std::wstring Text;
	};
	std::optional<Problem> Check(InfTemplate const& infTemplate, Values const& values);
}
