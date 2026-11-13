#include "TestCommon.h"
#include "InfFile.h"
#include "InstalledDrivers.h"

#include <algorithm>

namespace {
	constexpr char Package[] =
		"[Version]\r\n"
		"Signature   = \"$Windows NT$\"\r\n"
		"Class       = System\r\n"
		"ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}\r\n"
		"Provider    = %Mfg%\r\n"
		"DriverVer   = 03/07/2026,1.2.3.4\r\n"
		"CatalogFile = contoso.cat\r\n"
		"[Strings]\r\n"
		"Mfg = \"Contoso; Ltd\"\r\n";

	// a folder of its own in the temporary directory, deleted afterwards
	struct TempDir {
		std::filesystem::path Path = std::filesystem::temp_directory_path() / (L"InfStudioTests-" + std::to_wstring(::GetCurrentProcessId()) + L"-drivers");
		TempDir() {
			std::filesystem::create_directories(Path);
		}
		~TempDir() {
			std::error_code ec;
			std::filesystem::remove_all(Path, ec);
		}
		void Write(PCWSTR name, std::string_view text) const {
			std::ofstream(Path / name, std::ios::binary).write(text.data(), text.size());
		}
	};
}

TEST_CASE("DriverVer's date and version", "[drivers]") {
	auto [date, version] = InstalledDrivers::SplitDriverVer(L"03/07/2026,1.2.3.4");
	CHECK(date == L"2026-03-07");
	CHECK(version == L"1.2.3.4");
	// blanks, a missing version, one-digit month and day
	std::tie(date, version) = InstalledDrivers::SplitDriverVer(L" 3/7/2026 , 10.0.1 ");
	CHECK(date == L"2026-03-07");
	CHECK(version == L"10.0.1");
	std::tie(date, version) = InstalledDrivers::SplitDriverVer(L"06/21/2006");
	CHECK(date == L"2006-06-21");
	CHECK(version.empty());
	// not a date: as it is
	std::tie(date, version) = InstalledDrivers::SplitDriverVer(L"soon,1.0");
	CHECK(date == L"soon");
	CHECK(version == L"1.0");
	std::tie(date, version) = InstalledDrivers::SplitDriverVer(L"");
	CHECK(date.empty());
	CHECK(version.empty());
}

TEST_CASE("A driver package's details", "[drivers]") {
	TempDir dir;
	dir.Write(L"contoso.inf", Package);
	auto package = InstalledDrivers::Read(dir.Path / L"contoso.inf");
	CHECK(package.Valid);
	CHECK(package.PublishedName == L"contoso.inf");
	CHECK(package.Provider == L"Contoso; Ltd");
	CHECK(package.Class == L"System");
	CHECK(package.ClassGuid == L"{4d36e97d-e325-11ce-bfc1-08002be10318}");
	CHECK(package.Date == L"2026-03-07");
	CHECK(package.Version == L"1.2.3.4");
	CHECK_FALSE(package.ThirdParty);
	CHECK(package.Path == (dir.Path / L"contoso.inf").wstring());

	// published by Windows as oemNN.inf: a third party's
	dir.Write(L"oem42.inf", Package);
	CHECK(InstalledDrivers::Read(dir.Path / L"oem42.inf").ThirdParty);
	CHECK(InstalledDrivers::IsThirdParty(L"OEM7.INF"));
	CHECK_FALSE(InstalledDrivers::IsThirdParty(L"oemx.inf"));
	CHECK_FALSE(InstalledDrivers::IsThirdParty(L"oem.inf"));
	CHECK_FALSE(InstalledDrivers::IsThirdParty(L"usbport.inf"));

	// not an INF
	dir.Write(L"bad.inf", "nothing here\r\n");
	auto bad = InstalledDrivers::Read(dir.Path / L"bad.inf");
	CHECK_FALSE(bad.Valid);
	CHECK(bad.PublishedName == L"bad.inf");
}

TEST_CASE("The driver packages of a folder", "[drivers]") {
	TempDir dir;
	dir.Write(L"b.inf", Package);
	dir.Write(L"oem3.inf", Package);
	dir.Write(L"A.inf", Package);
	dir.Write(L"notes.txt", "not an INF");
	size_t progress = 0, total = 0;
	auto packages = InstalledDrivers::ReadAll(dir.Path, [&](size_t done, size_t count) {
		progress = done;
		total = count;
		return true;
	});
	REQUIRE(packages.size() == 3);
	// sorted by name, whatever its case
	CHECK(packages[0].PublishedName == L"A.inf");
	CHECK(packages[1].PublishedName == L"b.inf");
	CHECK(packages[2].PublishedName == L"oem3.inf");
	CHECK(progress == 3);
	CHECK(total == 3);

	// stopped by the callback: what was read so far
	auto some = InstalledDrivers::ReadAll(dir.Path, [](size_t done, size_t) { return done < 1; });
	CHECK(some.size() == 1);
	CHECK(InstalledDrivers::ReadAll(dir.Path / L"nowhere", nullptr).empty());
}

TEST_CASE("The driver packages Windows has", "[drivers][system]") {
	auto dir = InstalledDrivers::InfDirectory();
	REQUIRE(std::filesystem::exists(dir));
	auto usbport = InstalledDrivers::Read(dir / L"usbport.inf");
	CHECK(usbport.Valid);
	CHECK(usbport.Provider == L"Microsoft");
	CHECK(usbport.Class == L"USB");
	CHECK_FALSE(usbport.ThirdParty);
	// an inbox INF's package is in the driver store too
	CHECK(usbport.StorePath.find(L"FileRepository") != std::wstring::npos);

	// a third party's (if there's one): its original name, and its folder in the driver store
	for (auto const& entry : std::filesystem::directory_iterator(dir)) {
		auto name = entry.path().filename().wstring();
		if (!InstalledDrivers::IsThirdParty(name))
			continue;
		auto package = InstalledDrivers::Read(entry.path());
		CAPTURE(entry.path().filename().string());
		if (!package.Valid)
			continue;
		CHECK_FALSE(package.OriginalName.empty());
		CHECK(_wcsicmp(package.OriginalName.c_str(), name.c_str()) != 0);
		CHECK(package.StorePath.find(L"FileRepository") != std::wstring::npos);
		break;
	}
}
