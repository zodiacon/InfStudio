#include "TestCommon.h"
#include "InfNavigation.h"

namespace {
	const std::string Text =
		"[Version]\r\n"									// 0
		"Provider = %Mfg%\r\n"							// 1
		"[Manufacturer]\r\n"							// 2
		"%Mfg% = Contoso, NTamd64\r\n"					// 3
		"[Contoso.NTamd64]\r\n"							// 4
		"%Dev% = Install, PCI\\VEN_1234 ; %Mfg%\r\n"	// 5
		"[Install.NT]\r\n"								// 6
		"CopyFiles = Drivers, Missing\r\n"				// 7
		"Path = \"%ProgramFiles%\\Contoso\"\r\n"		// 8
		"[Drivers]\r\n"									// 9
		"a.sys\r\n"										// 10
		"[DestinationDirs]\r\n"							// 11
		"Drivers = 13\r\n"								// 12
		"[Strings]\r\n"									// 13
		"MFG = \"Contoso; Ltd\"\r\n"					// 14
		"Dev = \"Say \"\"hi\"\"\" ; the device\r\n"		// 15
		"ProgramFiles = C:\\Program Files\r\n"			// 16
		"[Strings.0407]\r\n"							// 17
		"Mfg = \"Contoso GmbH\"\r\n"					// 18
		"Only = \"Nur hier\"\r\n";						// 19

	// where the n-th occurrence of what is (plus offset into it)
	size_t At(std::string_view what, size_t offset = 0, int occurrence = 1) {
		size_t pos = 0;
		for (int i = 0; i < occurrence; i++) {
			pos = Text.find(what, i == 0 ? 0 : pos + 1);
			REQUIRE(pos != std::string::npos);
		}
		return pos + offset;
	}

	// the start of a line
	size_t Line(int line) {
		size_t pos = 0;
		for (int i = 0; i < line; i++)
			pos = Text.find('\n', pos) + 1;
		return pos;
	}
}

TEST_CASE("A string key's definition", "[navigation]") {
	// anywhere on %Mfg%, its percents included: the key in [Strings] (not the localized one), whatever its case
	for (size_t offset : { 0, 1, 3, 4 })
		CHECK(InfNavigation::FindDefinition(Text, At("%Mfg%", offset)) == Line(14));
	// as a key, too
	CHECK(InfNavigation::FindDefinition(Text, At("%Mfg% =", 2)) == Line(14));
	// in a string
	CHECK(InfNavigation::FindDefinition(Text, At("%ProgramFiles%", 3)) == Line(16));
	// only in a localized [Strings]: there
	auto only = Text + "X = %Only%\r\n";
	CHECK(InfNavigation::FindDefinition(only, only.find("%Only%") + 2) == Line(19));
	// in a comment it's nothing
	CHECK(InfNavigation::FindDefinition(Text, At("%Mfg%", 2, 3)) == std::string::npos);
	// not defined
	auto undefined = Text + "X = %Nowhere%\r\n";
	CHECK(InfNavigation::FindDefinition(undefined, undefined.find("%Nowhere%") + 2) == std::string::npos);
}

TEST_CASE("A section's definition", "[navigation]") {
	// a models section a Manufacturer value names isn't there by that name (the decorations are added): nothing
	CHECK(InfNavigation::FindDefinition(Text, At("Contoso, NTamd64", 2)) == std::string::npos);
	// sections a value names
	CHECK(InfNavigation::FindDefinition(Text, At("Drivers, Missing", 3)) == Line(9));
	CHECK(InfNavigation::FindDefinition(Text, At("Missing", 0)) == std::string::npos);
	// a section a key names ([DestinationDirs])
	CHECK(InfNavigation::FindDefinition(Text, At("Drivers = 13", 0)) == Line(9));
	// the install section of a device: the first field, without the decorations; [Install.NT] has them
	CHECK(InfNavigation::FindDefinition(Text, At("Install, PCI", 1)) == std::string::npos);
	// a section's own header, a directive, a number, a blank: nothing
	CHECK(InfNavigation::FindDefinition(Text, At("[Drivers]", 2)) == std::string::npos);
	CHECK(InfNavigation::FindDefinition(Text, At("CopyFiles", 2)) == std::string::npos);
	CHECK(InfNavigation::FindDefinition(Text, At("13", 0)) == std::string::npos);
	CHECK(InfNavigation::FindDefinition(Text, At("CopyFiles = ", 10)) == std::string::npos);
	// out of the text
	CHECK(InfNavigation::FindDefinition(Text, Text.size() + 10) == std::string::npos);
	CHECK(InfNavigation::FindDefinition("", 0) == std::string::npos);
}

TEST_CASE("A string key's value", "[navigation]") {
	CHECK(InfNavigation::StringValueAt(Text, At("%Mfg%", 2)) == "Contoso; Ltd");
	// quotes taken off, "" a quote, the comment left out
	CHECK(InfNavigation::StringValueAt(Text, At("%Dev%", 1)) == "Say \"hi\"");
	// not quoted
	CHECK(InfNavigation::StringValueAt(Text, At("%ProgramFiles%", 1)) == "C:\\Program Files");
	// not a string key, or not defined
	CHECK_FALSE(InfNavigation::StringValueAt(Text, At("Drivers, Missing", 1)).has_value());
	auto undefined = Text + "X = %Nowhere%\r\n";
	CHECK_FALSE(InfNavigation::StringValueAt(undefined, undefined.find("%Nowhere%") + 1).has_value());
}

TEST_CASE("A string key's definition with LF line ends", "[navigation]") {
	std::string text = "[Version]\nProvider = %Mfg%\n[Strings]\nMfg = \"Contoso\"\n";
	CHECK(InfNavigation::FindDefinition(text, text.find("%Mfg%") + 1) == text.find("Mfg ="));
	CHECK(InfNavigation::StringValueAt(text, text.find("%Mfg%") + 1) == "Contoso");
}

TEST_CASE("A one-letter string key", "[navigation]") {
	std::string text =
		"[Manufacturer]\r\n"
		"%A% = Plain\r\n"
		"[Plain]\r\n"
		"%D% = Install, ID1\r\n"
		"[Strings]\r\n"
		"A = \"A Inc\"\r\n"
		"D = \"Device\"\r\n";
	auto pos = text.find("%D%") + 1;
	CHECK(InfNavigation::StringValueAt(text, pos) == "Device");
	CHECK(InfNavigation::FindDefinition(text, pos) == text.find("D = "));
}

namespace {
	// INFs an INF can include, by file name (whatever its case)
	struct Includable {
		std::vector<std::pair<std::string, std::string>> Files = {
			{ "machine.inf", "[Version]\r\nSignature = \"$Windows NT$\"\r\n[PCI_DRV]\r\nAddReg = PCI_AddReg\r\n[PCI_DRV.Services]\r\nAddService = pci, 2, pci_svc\r\n[PCI_AddReg]\r\n[Strings]\r\nA = \"a\"\r\n[Strings.0407]\r\nA = \"b\"\r\n[SourceDisksNames.amd64]\r\n1 = x\r\n" },
			{ "usb.inf", "[USB.Dev.NT]\r\nAddReg = y\r\n" },
		};
		std::string const* operator()(std::string_view file) const {
			for (auto const& [name, text] : Files)
				if (_strnicmp(name.c_str(), file.data(), file.size()) == 0 && name.size() == file.size())
					return &text;
			return nullptr;
		}
	};
}

TEST_CASE("A section's header", "[navigation]") {
	CHECK(InfNavigation::FindSection(Text, "drivers") == Line(9));
	CHECK(InfNavigation::FindSection(Text, "Strings.0407") == Line(17));
	CHECK(InfNavigation::FindSection(Text, "Nowhere") == std::string::npos);
	CHECK(InfNavigation::FindSection("", "Version") == std::string::npos);
}

TEST_CASE("Include and Needs: what they name in other INFs", "[navigation]") {
	const std::string text =
		"[Install.NT]\r\n"
		"Include = machine.inf, USB.INF, nothere.inf\r\n"
		"Needs = PCI_DRV, USB.Dev.NT, Nowhere ; pci_drv\r\n"
		"CopyFiles = PCI_DRV\r\n"
		"[Install.NT.Services]\r\n"
		"Include = machine.inf\r\n"
		"Needs = pci_drv.services, USB.Dev.NT\r\n"
		"[Other]\r\n"
		"Needs = USB.Dev.NT\r\n";
	Includable included;
	auto at = [&](std::string_view what, int occurrence = 1) {
		size_t pos = std::string::npos;
		for (int i = 0; i < occurrence; i++)
			pos = text.find(what, pos == std::string::npos ? 0 : pos + 1);
		REQUIRE(pos != std::string::npos);
		return InfNavigation::FindIncluded(text, pos + 1, included);
	};
	auto is = [](std::optional<InfNavigation::IncludedTarget> const& t, std::string_view file, std::string_view section, bool found) {
		return t && t->File == file && t->Section == section && t->Found == found;
	};

	// an Include's file: the INF (if it's there)
	CHECK(is(at("machine.inf"), "machine.inf", "", true));
	CHECK(is(at("USB.INF"), "USB.INF", "", true));
	CHECK(is(at("nothere.inf"), "nothere.inf", "", false));

	// a Needs' section: in the first of the INFs the section includes that has it (whatever its case)
	CHECK(is(at("PCI_DRV"), "machine.inf", "PCI_DRV", true));
	CHECK(is(at("USB.Dev.NT"), "USB.INF", "USB.Dev.NT", true));
	CHECK(is(at("pci_drv.services"), "machine.inf", "pci_drv.services", true));
	CHECK(is(at("Nowhere"), "", "Nowhere", false));
	// a section with an Include of its own looks only there
	CHECK(is(at("USB.Dev.NT", 2), "", "USB.Dev.NT", false));
	// one without: in every INF the file includes
	CHECK(is(at("USB.Dev.NT", 3), "USB.INF", "USB.Dev.NT", true));

	// not: the key, a comment, another directive; nothing to read the other INFs with
	CHECK_FALSE(at("Needs"));
	CHECK_FALSE(at("pci_drv"));
	CHECK_FALSE(at("PCI_DRV", 2));
	CHECK(is(InfNavigation::FindIncluded(text, text.find("PCI_DRV") + 1, {}), "", "PCI_DRV", false));
}
