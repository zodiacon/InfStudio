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

using Sections = std::vector<std::wstring>;

TEST_CASE("The models sections a Manufacturer value names", "[inffile]") {
	// decorated: one section per decoration
	CHECK(InfFile::GetModelSections(L"Contoso,NTamd64,NTarm64") == Sections{ L"Contoso.NTamd64", L"Contoso.NTarm64" });
	CHECK(InfFile::GetModelSections(L"Contoso,NTamd64.10.0...16299") == Sections{ L"Contoso.NTamd64.10.0...16299" });
	// undecorated: the section is the name itself (these were left out of the tree)
	CHECK(InfFile::GetModelSections(L"Contoso") == Sections{ L"Contoso" });
	// blanks around the fields don't count, and empty decorations name nothing
	CHECK(InfFile::GetModelSections(L" Contoso , NTamd64 ") == Sections{ L"Contoso.NTamd64" });
	CHECK(InfFile::GetModelSections(L"Contoso,,NTamd64,") == Sections{ L"Contoso.NTamd64" });
	CHECK(InfFile::GetModelSections(L"Contoso,") == Sections{ L"Contoso" });
	// no name: nothing
	CHECK(InfFile::GetModelSections(L"").empty());
	CHECK(InfFile::GetModelSections(L" ,NTamd64").empty());
}

TEST_CASE("Models sections of a real Manufacturer section", "[inffile]") {
	TempFile file(L"manufacturers.inf");
	file.Write(
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"[Manufacturer]\r\n"
		"%A% = Plain\r\n"
		"%B% = Decorated, NTamd64, NTarm64\r\n"
		"[Plain]\r\n"
		"%D% = Install, ID1\r\n"
		"[Decorated.NTamd64]\r\n"
		"%D% = Install, ID2\r\n"
		"[Decorated.NTarm64]\r\n"
		"%D% = Install, ID3\r\n"
		"[Strings]\r\n"
		"A = \"A Inc\"\r\n"
		"B = \"B Inc\"\r\n"
		"D = \"Device\"\r\n");
	InfFile inf;
	REQUIRE(inf.Open(file.c_str()));

	Sections all;
	for (auto const& [key, value] : inf.GetSectionCompactLines(L"Manufacturer"))
		for (auto& section : InfFile::GetModelSections(value))
			all.push_back(section);
	CHECK(all == Sections{ L"Plain", L"Decorated.NTamd64", L"Decorated.NTarm64" });
	for (auto const& section : all)
		CHECK(inf.SectionExists(section.c_str()));
}

TEST_CASE("The sections an install section can be", "[inffile]") {
	// the platform decorations Windows picks from, each with its .HW section
	auto names = InfFile::InstallSectionNames(L"Install");
	auto has = [&](PCWSTR name) { return std::find(names.begin(), names.end(), name) != names.end(); };
	for (auto name : { L"Install", L"Install.NT", L"Install.NTamd64", L"Install.NTarm64", L"Install.NTx86",
		L"Install.HW", L"Install.NT.HW", L"Install.NTamd64.HW", L"Install.NTarm64.HW" })
		CHECK(has(name));
	// .HW goes after the platform, not before it
	CHECK_FALSE(has(L"Install.HW.NT"));
	CHECK(InfFile::InstallSectionNames(L"").empty());
}

TEST_CASE("The directives of an install section, with its decorations", "[inffile]") {
	TempFile file(L"decorations.inf");
	file.Write(
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"[Install]\r\n"
		"CopyFiles = Legacy.Files\r\n"
		"[Install.NTamd64]\r\n"
		"CopyFiles = Amd64.Files, Common.Files\r\n"
		"DelFiles  = Old.Files\r\n"
		"[Install.NTarm64]\r\n"
		"CopyFiles = Arm64.Files,, Common.Files\r\n"
		"[Install.NTamd64.HW]\r\n"
		"AddReg = Hw.AddReg\r\n"
		"DelReg = Hw.DelReg\r\n"
		"Include = other.inf\r\n"
		"[Amd64.Files]\r\n"
		"a.sys\r\n");
	InfFile inf;
	REQUIRE(inf.Open(file.c_str()));

	auto directives = inf.GetInstallDirectives(L"Install");
	std::vector<std::wstring> found;
	for (auto const& d : directives)
		found.push_back(d.Section + L": " + d.Directive + L" " + d.Target);
	CHECK(found == std::vector<std::wstring>{
		L"Install: CopyFiles Legacy.Files",
		L"Install.NTamd64: CopyFiles Amd64.Files",
		L"Install.NTamd64: CopyFiles Common.Files",
		L"Install.NTamd64: DelFiles Old.Files",
		L"Install.NTamd64.HW: AddReg Hw.AddReg",
		L"Install.NTamd64.HW: DelReg Hw.DelReg",
		L"Install.NTarm64: CopyFiles Arm64.Files",
		L"Install.NTarm64: CopyFiles Common.Files",
	});
	CHECK(inf.GetInstallDirectives(L"NoSuchSection").empty());
}
