#include "TestCommon.h"
#include "InfTemplates.h"
#include "InfLexer.h"
#include "InfFile.h"
#include "TextFile.h"
#include "InfVerif.h"

#include <algorithm>
#include <cwctype>
#include <set>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace {
	// 2026-03-07 and a GUID that is easy to find
	InfTemplates::Values TestValues() {
		InfTemplates::Values values{};
		values.Date.wYear = 2026;
		values.Date.wMonth = 3;
		values.Date.wDay = 7;
		values.Guid = { 0x12345678, 0x9abc, 0xdef0, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } };
		return values;
	}

	std::wstring Lower(std::wstring text) {
		std::transform(text.begin(), text.end(), text.begin(), [](wchar_t ch) { return (wchar_t)std::towlower(ch); });
		return text;
	}

	// the %strkey% names the text uses (by the lexer's Token style), outside [Strings]; not %13% and the like (DIRIDs)
	std::set<std::wstring> UsedStringKeys(std::string const& text) {
		std::set<std::wstring> keys;
		bool inStrings = false;
		size_t pos = 0;
		bool continued = false;
		while (pos < text.size()) {
			auto end = text.find("\r\n", pos);
			if (end == std::string::npos)
				end = text.size();
			std::string_view line(text.data() + pos, end - pos);
			std::vector<uint8_t> styles(line.size());
			continued = InfLexer::StyleLine(line, continued, styles);
			if (!line.empty() && styles[0] == InfLexer::Section)
				inStrings = _strnicmp(line.data(), "[Strings]", 9) == 0;
			else if (!inStrings) {
				for (size_t i = 0; i < line.size(); i++) {
					if (styles[i] != InfLexer::Token)
						continue;
					auto start = i;
					while (i < line.size() && styles[i] == InfLexer::Token && !(i > start && line[i] == '%'))
						i++;
					auto name = line.substr(start + 1, i - start - 1);
					if (!std::all_of(name.begin(), name.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
						keys.insert(Lower(std::wstring(name.begin(), name.end())));
				}
			}
			pos = end + 2;
		}
		return keys;
	}
}

TEST_CASE("GUIDs as text", "[templates]") {
	CHECK(InfTemplates::GuidText(TestValues().Guid) == "{12345678-9ABC-DEF0-0123-456789ABCDEF}");
	CHECK(InfTemplates::GuidText(GUID{}) == "{00000000-0000-0000-0000-000000000000}");
}

TEST_CASE("New values are today and a new GUID", "[templates]") {
	auto a = InfTemplates::NewValues();
	auto b = InfTemplates::NewValues();
	SYSTEMTIME now;
	::GetLocalTime(&now);
	CHECK(a.Date.wYear == now.wYear);
	CHECK(a.Date.wMonth == now.wMonth);
	CHECK(a.Guid != GUID{});
	CHECK(a.Guid != b.Guid);
}

TEST_CASE("The templates", "[templates]") {
	auto all = InfTemplates::All();
	REQUIRE(all.size() >= 8);
	std::set<std::wstring> names, files;
	for (auto const& t : all) {
		names.insert(t.Name);
		files.insert(InfTemplates::FileName(t, {}));
	}
	CHECK(names.size() == all.size());
	CHECK(files.size() == all.size());
	// sorted by group, so that the menu's separators come between groups
	CHECK(std::is_sorted(all.begin(), all.end(), [](auto const& a, auto const& b) { return a.Group < b.Group; }));
}

TEST_CASE("Every template makes a good INF", "[templates]") {
	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	CAPTURE(TextFile::ToUtf8(t.Name));

	auto values = TestValues();
	auto text = InfTemplates::Generate(t, values);

	// the placeholders are all filled in, the line ends are CRLF, and it's ASCII
	CHECK(text.find('@') == std::string::npos);
	CHECK(text.find("DriverVer   = 03/07/2026,1.0.0.0\r\n") != std::string::npos);
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == '\n')
			REQUIRE((i > 0 && text[i - 1] == '\r'));
		REQUIRE((unsigned char)text[i] < 0x80);
	}
	if (std::string_view(t.Text).find("@GUID@") != std::string_view::npos)
		CHECK(text.find(InfTemplates::GuidText(values.Guid)) != std::string::npos);

	// SetupAPI takes it, and every string key it uses is in [Strings]
	TempFile file(InfTemplates::FileName(t, values));
	file.Write(text);
	InfFile inf;
	UINT errorLine = 0;
	REQUIRE(inf.Open(file.c_str(), nullptr, &errorLine));
	CHECK(inf.GetStringValue(L"Version", L"Signature", 1) == L"$Windows NT$");
	CHECK(inf.GetStringValue(L"Version", L"CatalogFile", 1) == std::wstring(t.DriverName) + L".cat");

	std::set<std::wstring> defined;
	for (auto const& [key, value] : inf.GetSectionCompactLines(L"Strings"))
		defined.insert(Lower(key));
	auto used = UsedStringKeys(text);
	// (every template has a provider: a check that the keys are found at all)
	CHECK(used.contains(L"manufacturername"));
	CHECK_FALSE(used.contains(L"13"));
	for (auto const& key : used) {
		CAPTURE(TextFile::ToUtf8(key));
		CHECK(defined.contains(key));
	}

	// the models sections the Manufacturer section names are there
	for (auto const& [key, value] : inf.GetSectionCompactLines(L"Manufacturer"))
		for (auto const& section : InfFile::GetModelSections(value))
			CHECK(inf.SectionExists(section.c_str()));
}

TEST_CASE("Every template passes InfVerif", "[templates][infverif]") {
	auto infverif = InfVerif::Find();
	if (infverif.empty())
		SKIP("the WDK's infverif.exe isn't installed");

	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	TempFile file(InfTemplates::FileName(t, {}));
	file.Write(InfTemplates::Generate(t, InfTemplates::NewValues()));

	// /w: Windows Driver requirements
	auto result = InfVerif::Run(infverif, L"/w", file.Path());
	CAPTURE(result.Output);
	REQUIRE(result.Ran);
	CHECK(result.Messages.empty());
	CHECK(result.ExitCode == 0);
}

namespace {
	// what a user might type in the wizard: a quote and a percent in the quoted strings
	InfTemplates::Values CustomValues(InfTemplate const& t) {
		auto values = TestValues();
		values.DriverName = L"AcmeWidget2";
		values.Manufacturer = L"Acme \"Labs\" 100%";
		values.Description = L"Acme Widget, 2nd edition";
		values.HardwareId = t.HardwareId ? L"PCI\\VEN_ABCD&DEV_0042&SUBSYS_00000000" : L"";
		values.Version = L"2.5.0.0";
		return values;
	}
}

TEST_CASE("The defaults are what the templates are without values", "[templates]") {
	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	auto defaults = InfTemplates::DefaultValues(t);
	CHECK_FALSE(InfTemplates::Check(t, defaults));
	CHECK(defaults.Manufacturer == L"Contoso");
	CHECK(defaults.Version == L"1.0.0.0");
	CHECK(defaults.HardwareId.empty() == (t.HardwareId == nullptr));

	auto empty = TestValues();
	auto filled = defaults;
	filled.Date = empty.Date;
	filled.Guid = empty.Guid;
	CHECK(InfTemplates::Generate(t, filled) == InfTemplates::Generate(t, empty));
	CHECK(InfTemplates::FileName(t, defaults) == std::wstring(t.DriverName) + L".inf");
}

TEST_CASE("A template with the wizard's values", "[templates]") {
	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	CAPTURE(TextFile::ToUtf8(t.Name));
	auto values = CustomValues(t);
	REQUIRE_FALSE(InfTemplates::Check(t, values));
	auto text = InfTemplates::Generate(t, values);

	// none of the defaults left
	CHECK(text.find('@') == std::string::npos);
	CHECK(text.find("Contoso") == std::string::npos);
	CHECK(text.find(TextFile::ToUtf8(t.DriverName)) == std::string::npos);
	if (t.HardwareId)
		CHECK(text.find(TextFile::ToUtf8(t.HardwareId)) == std::string::npos);
	CHECK(text.find("DriverVer   = 03/07/2026,2.5.0.0\r\n") != std::string::npos);

	TempFile file(InfTemplates::FileName(t, values));
	CHECK(file.Path().filename().wstring().ends_with(L"-AcmeWidget2.inf"));
	file.Write(text);
	InfFile inf;
	UINT errorLine = 0;
	REQUIRE(inf.Open(file.c_str(), nullptr, &errorLine));
	CHECK(inf.GetStringValue(L"Version", L"CatalogFile", 1) == L"AcmeWidget2.cat");
	// the quote escaped (the percent isn't, in [Strings]): SetupAPI gives them back as typed
	CHECK(inf.GetStringValue(L"Strings", L"ManufacturerName", 1) == values.Manufacturer);
	CHECK(inf.GetStringValue(L"Version", L"Provider", 1) == values.Manufacturer);
	// the hardware ID a models section installs on
	if (t.HardwareId) {
		bool found = false;
		for (auto const& [key, value] : inf.GetSectionCompactLines(L"Manufacturer"))
			for (auto const& section : InfFile::GetModelSections(value))
				for (auto const& [desc, install] : inf.GetSectionCompactLines(section.c_str()))
					found |= install.find(values.HardwareId) != std::wstring::npos;
		CHECK(found);
	}
}

TEST_CASE("Templates with the wizard's values pass InfVerif", "[templates][infverif]") {
	auto infverif = InfVerif::Find();
	if (infverif.empty())
		SKIP("the WDK's infverif.exe isn't installed");

	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	auto values = CustomValues(t);
	TempFile file(InfTemplates::FileName(t, values));
	file.Write(InfTemplates::Generate(t, values));
	auto result = InfVerif::Run(infverif, L"/w", file.Path());
	CAPTURE(result.Output);
	REQUIRE(result.Ran);
	CHECK(result.Messages.empty());
	CHECK(result.ExitCode == 0);
}

TEST_CASE("The wizard's values are checked", "[templates]") {
	auto const& device = InfTemplates::All()[0];
	REQUIRE(device.HardwareId);
	auto good = InfTemplates::DefaultValues(device);
	auto problem = [&](auto change) {
		auto values = good;
		change(values);
		auto found = InfTemplates::Check(device, values);
		return found ? std::optional(found->Field) : std::nullopt;
	};
	using F = InfTemplates::Field;
	CHECK(problem([](auto& v) { v.DriverName = L""; }) == F::DriverName);
	CHECK(problem([](auto& v) { v.DriverName = L"2Fast"; }) == F::DriverName);
	CHECK(problem([](auto& v) { v.DriverName = L"My Driver"; }) == F::DriverName);
	CHECK(problem([](auto& v) { v.DriverName = L"My-Driver"; }) == F::DriverName);
	CHECK(problem([](auto& v) { v.DriverName = std::wstring(65, L'a'); }) == F::DriverName);
	CHECK(problem([](auto& v) { v.DriverName = L"_my_driver64"; }) == std::nullopt);

	CHECK(problem([](auto& v) { v.Manufacturer = L""; }) == F::Manufacturer);
	CHECK(problem([](auto& v) { v.Manufacturer = L"Two\r\nlines"; }) == F::Manufacturer);
	CHECK(problem([](auto& v) { v.Description = L""; }) == F::Description);
	CHECK(problem([](auto& v) { v.Description = L"Caf\u00e9 \"quoted\" 50%"; }) == std::nullopt);

	CHECK(problem([](auto& v) { v.HardwareId = L""; }) == F::HardwareId);
	CHECK(problem([](auto& v) { v.HardwareId = L"PCI\\VEN_1234 DEV_5678"; }) == F::HardwareId);
	CHECK(problem([](auto& v) { v.HardwareId = L"PCI\\VEN_1234,DEV_5678"; }) == F::HardwareId);
	CHECK(problem([](auto& v) { v.HardwareId = L"PCI\\VEN_%1234%"; }) == F::HardwareId);
	CHECK(problem([](auto& v) { v.HardwareId = L"ACPI\\PNP0C0A"; }) == std::nullopt);

	CHECK(problem([](auto& v) { v.Version = L""; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"1.0.0.0.0"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"1..0"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"1.0."; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"1.65536"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"v1"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"10"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"1.2.3"; }) == F::Version);
	CHECK(problem([](auto& v) { v.Version = L"65535.0.12.4"; }) == std::nullopt);

	// a template without a hardware ID doesn't need one
	auto service = std::find_if(InfTemplates::All().begin(), InfTemplates::All().end(), [](auto const& t) { return !t.HardwareId; });
	REQUIRE(service != InfTemplates::All().end());
	auto values = InfTemplates::DefaultValues(*service);
	CHECK(values.HardwareId.empty());
	CHECK_FALSE(InfTemplates::Check(*service, values));
}
