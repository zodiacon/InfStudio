#pragma once

#include <string>
#include <span>

//
// The INFs File > New from Template makes. A template's text has placeholders that Generate fills in:
// @DATE@ (DriverVer's date) and @GUID@ (a new GUID, for what must be unique to the package: a device interface,
// a NetCfgInstanceId, an ExtensionId).
//
struct InfTemplate {
	PCWSTR Name;		// in the menu
	PCWSTR FileName;	// what Save As suggests
	int Group;			// the menu has a separator between groups
	const char* Text;
};

namespace InfTemplates {
	std::span<InfTemplate const> All();

	struct Values {
		SYSTEMTIME Date;
		GUID Guid;
	};
	// today, and a new GUID
	Values NewValues();
	// the template's text with its placeholders filled in, with CRLF line ends
	std::string Generate(InfTemplate const& infTemplate, Values const& values);
	// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}, upper case
	std::string GuidText(GUID const& guid);
}
