#include "TestCommon.h"
#include "InfTemplates.h"
#include "InfLexer.h"
#include "InfFile.h"
#include "TextFile.h"

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

	// infverif.exe of the newest WDK, or empty
	std::filesystem::path FindInfVerif() {
		std::filesystem::path tools = LR"(C:\Program Files (x86)\Windows Kits\10\Tools)";
		std::error_code ec;
		std::filesystem::path found;
		for (auto const& dir : std::filesystem::directory_iterator(tools, ec)) {
			auto exe = dir.path() / L"x64" / L"infverif.exe";
			if (std::filesystem::exists(exe, ec) && (found.empty() || exe > found))
				found = exe;
		}
		return found;
	}

	struct ProcessResult {
		int ExitCode;
		std::string Output;
	};

	ProcessResult Run(std::wstring const& commandLine) {
		std::wstring command = L"\"" + commandLine + L" 2>&1\"";
		ProcessResult result{ -1 };
		auto pipe = _wpopen(command.c_str(), L"rb");
		if (!pipe)
			return result;
		char buffer[512];
		size_t read;
		while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
			result.Output.append(buffer, read);
		result.ExitCode = _pclose(pipe);
		return result;
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
		files.insert(t.FileName);
	}
	CHECK(names.size() == all.size());
	CHECK(files.size() == all.size());
	// sorted by group, so that the menu's separators come between groups
	CHECK(std::is_sorted(all.begin(), all.end(), [](auto const& a, auto const& b) { return a.Group < b.Group; }));
}

TEST_CASE("Every template makes a good INF", "[templates]") {
	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	CAPTURE(TextFile::ToUtf8(t.FileName));

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
	TempFile file(t.FileName);
	file.Write(text);
	InfFile inf;
	UINT errorLine = 0;
	REQUIRE(inf.Open(file.c_str(), nullptr, &errorLine));
	CHECK(inf.GetStringValue(L"Version", L"Signature", 1) == L"$Windows NT$");
	CHECK(inf.GetStringValue(L"Version", L"CatalogFile", 1) == std::filesystem::path(t.FileName).replace_extension(L".cat").wstring());

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
	auto infverif = FindInfVerif();
	if (infverif.empty())
		SKIP("the WDK's infverif.exe isn't installed");

	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	TempFile file(t.FileName);
	file.Write(InfTemplates::Generate(t, InfTemplates::NewValues()));

	// /w: Windows Driver requirements
	auto result = Run(L"\"" + infverif.wstring() + L"\" /w \"" + file.Path().wstring() + L"\"");
	CAPTURE(result.Output);
	CHECK(result.ExitCode == 0);
}
