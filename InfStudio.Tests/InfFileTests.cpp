#include "TestCommon.h"
#include "InfFile.h"

#include <algorithm>

namespace {
	constexpr char SampleInf[] =
		"; a sample\r\n"
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"Class     = USB ; a comment\r\n"
		"Provider  = %Mfg%\r\n"
		"\r\n"
		"[Manufacturer]\r\n"
		"%Mfg% = Contoso,NTamd64,NTarm64\r\n"
		"\r\n"
		"[Contoso.NTamd64]\r\n"
		"%Desc% = Install, USB\\VID_1234&PID_5678\r\n"
		"\r\n"
		"[Install.NT]\r\n"
		"CopyFiles = Drivers\r\n"
		"AddReg    = Install.AddReg\r\n"
		"\r\n"
		"[Install.AddReg]\r\n"
		"HKR,,Security,,\"D:P(A;;GA;;;SY)\"\r\n"
		"\r\n"
		"[Strings]\r\n"
		"Mfg  = \"Contoso; Ltd\"\r\n"
		"Desc = \"Contoso Device\"\r\n";

	bool Contains(std::vector<std::wstring> const& names, std::wstring_view name) {
		return std::any_of(names.begin(), names.end(), [&](auto const& n) { return _wcsicmp(n.c_str(), std::wstring(name).c_str()) == 0; });
	}
}

TEST_CASE("Opening an INF file", "[inffile]") {
	TempFile file(L"sample.inf");
	file.Write(SampleInf);
	InfFile inf;
	UINT errorLine = 0;
	REQUIRE(inf.Open(file.c_str(), nullptr, &errorLine));
	CHECK(inf.IsValid());
	CHECK(inf.GetFileCount() == 1);

	auto sections = inf.GetSectionNames();
	for (auto name : { L"Version", L"Manufacturer", L"Contoso.NTamd64", L"Install.NT", L"Install.AddReg", L"Strings" })
		CHECK(Contains(sections, name));
	CHECK(inf.SectionExists(L"Install.NT"));
	CHECK_FALSE(inf.SectionExists(L"NoSuchSection"));

	// values come with their comments taken off and their string keys replaced
	CHECK(inf.GetStringValue(L"Version", L"Class", 1) == L"USB");
	CHECK(inf.GetStringValue(L"Version", L"Provider", 1) == L"Contoso; Ltd");
	CHECK(inf.GetStringValue(L"Install.AddReg", nullptr, 5) == L"D:P(A;;GA;;;SY)");
	CHECK(inf.GetStringValue(L"Version", L"NoSuchKey", 1).empty());

	auto version = inf.GetVersionSectionInfo();
	CHECK(std::any_of(version.begin(), version.end(), [](auto const& kv) { return _wcsicmp(kv.first.c_str(), L"Class") == 0 && kv.second == L"USB"; }));

	inf.Close();
	CHECK_FALSE(inf.IsValid());
}

TEST_CASE("Section lines", "[inffile]") {
	TempFile file(L"lines.inf");
	file.Write(SampleInf);
	InfFile inf;
	REQUIRE(inf.Open(file.c_str()));

	auto lines = inf.GetSectionCompactLines(L"Install.NT");
	REQUIRE(lines.size() == 2);
	CHECK(lines[0].first == L"CopyFiles");
	CHECK(lines[0].second == L"Drivers");
	CHECK(lines[1].first == L"AddReg");
	CHECK(lines[1].second == L"Install.AddReg");

	CHECK(inf.GetSectionLines(L"Install.NT").size() == 2);
	CHECK(inf.GetSectionCompactLines(L"NoSuchSection").empty());
}

TEST_CASE("A UTF-16 INF file", "[inffile]") {
	TempFile file(L"unicode.inf");
	std::wstring text = L"[Version]\r\nSignature = \"$Windows NT$\"\r\nProvider = %Mfg%\r\n[Strings]\r\nMfg = \"Caf\u00E9 \u05E9\u05DC\u05D5\u05DD\"\r\n";
	std::string bytes = "\xFF\xFE";
	bytes.append((const char*)text.data(), text.size() * sizeof(wchar_t));
	file.Write(bytes);
	InfFile inf;
	REQUIRE(inf.Open(file.c_str()));
	CHECK(inf.GetStringValue(L"Version", L"Provider", 1) == L"Caf\u00E9 \u05E9\u05DC\u05D5\u05DD");
}

TEST_CASE("A file that isn't an INF file", "[inffile]") {
	TempFile file(L"bad.inf");
	file.Write("no sections here\r\n");
	InfFile inf;
	CHECK_FALSE(inf.Open(file.c_str()));
	CHECK_FALSE(inf.IsValid());

	TempFile missing(L"missing.inf");
	CHECK_FALSE(inf.Open(missing.c_str()));
}

TEST_CASE("Decorated model sections", "[inffile]") {
	// a Manufacturer line's value: the models section and its decorations
	auto pairs = InfFile::GetStringPairs(L"Contoso,NTamd64,NTarm64");
	REQUIRE(pairs.size() == 2);
	CHECK(pairs[0] == L"Contoso.NTamd64");
	CHECK(pairs[1] == L"Contoso.NTarm64");
	CHECK(InfFile::GetStringPairs(L"Contoso,NTamd64") == std::vector<std::wstring>{ L"Contoso.NTamd64" });
}
