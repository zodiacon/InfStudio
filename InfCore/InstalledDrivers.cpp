#include "pch.h"
#include "InstalledDrivers.h"
#include "InfFile.h"

#include <algorithm>
#include <cwctype>
#include <format>

namespace {
	std::wstring_view Trim(std::wstring_view text) noexcept {
		auto first = text.find_first_not_of(L" \t");
		if (first == std::wstring_view::npos)
			return {};
		return text.substr(first, text.find_last_not_of(L" \t") - first + 1);
	}

	bool AllDigits(std::wstring_view text) noexcept {
		return !text.empty() && std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; });
	}
}

std::filesystem::path InstalledDrivers::InfDirectory() {
	WCHAR windows[MAX_PATH];
	if (!::GetWindowsDirectory(windows, _countof(windows)))
		return {};
	return std::filesystem::path(windows) / L"INF";
}

bool InstalledDrivers::IsThirdParty(std::wstring_view name) {
	if (name.size() < 8 || _wcsnicmp(name.data(), L"oem", 3) != 0 || _wcsicmp(name.data() + name.size() - 4, L".inf") != 0)
		return false;
	return AllDigits(name.substr(3, name.size() - 7));
}

std::pair<std::wstring, std::wstring> InstalledDrivers::SplitDriverVer(std::wstring_view driverVer) {
	auto comma = driverVer.find(L',');
	auto date = Trim(driverVer.substr(0, comma));
	auto version = comma == std::wstring_view::npos ? std::wstring_view{} : Trim(driverVer.substr(comma + 1));

	// mm/dd/yyyy
	auto slash1 = date.find(L'/'), slash2 = date.rfind(L'/');
	if (slash1 != std::wstring_view::npos && slash2 != slash1) {
		auto month = date.substr(0, slash1), day = date.substr(slash1 + 1, slash2 - slash1 - 1), year = date.substr(slash2 + 1);
		if (AllDigits(month) && AllDigits(day) && AllDigits(year) && year.size() == 4 && month.size() <= 2 && day.size() <= 2)
			return { std::format(L"{}-{:0>2}-{:0>2}", year, month, day), std::wstring(version) };
	}
	return { std::wstring(date), std::wstring(version) };
}

DriverPackage InstalledDrivers::Read(std::filesystem::path const& inf) {
	DriverPackage package;
	package.PublishedName = inf.filename().wstring();
	package.Path = inf.wstring();
	package.ThirdParty = IsThirdParty(package.PublishedName);

	InfFile file;
	if (!file.Open(inf.c_str()))
		return package;
	package.Valid = true;
	package.Provider = file.GetStringValue(L"Version", L"Provider", 1);
	package.Class = file.GetStringValue(L"Version", L"Class", 1);
	package.ClassGuid = file.GetStringValue(L"Version", L"ClassGuid", 1);
	// (DriverVer's fields: the date, then the version)
	auto date = file.GetStringValue(L"Version", L"DriverVer", 1);
	auto version = file.GetStringValue(L"Version", L"DriverVer", 2);
	std::tie(package.Date, std::ignore) = SplitDriverVer(date);
	package.Version = version;
	if (package.ThirdParty) {
		auto original = file.GetOriginalFileName();
		if (_wcsicmp(original.c_str(), package.PublishedName.c_str()) != 0)
			package.OriginalName = original;
	}
	file.Close();

	// its copy in the driver store
	WCHAR store[MAX_PATH];
	if (::SetupGetInfDriverStoreLocation(inf.c_str(), nullptr, nullptr, store, _countof(store), nullptr))
		package.StorePath = store;
	return package;
}

std::vector<DriverPackage> InstalledDrivers::ReadAll(std::filesystem::path const& dir, std::function<bool(size_t, size_t)> const& progress) {
	std::vector<std::filesystem::path> files;
	std::error_code ec;
	for (auto const& entry : std::filesystem::directory_iterator(dir, ec))
		if (entry.is_regular_file(ec) && _wcsicmp(entry.path().extension().c_str(), L".inf") == 0)
			files.push_back(entry.path());
	std::sort(files.begin(), files.end(), [](auto const& a, auto const& b) {
		return _wcsicmp(a.filename().c_str(), b.filename().c_str()) < 0;
	});

	std::vector<DriverPackage> packages;
	packages.reserve(files.size());
	for (auto const& file : files) {
		packages.push_back(Read(file));
		if (progress && !progress(packages.size(), files.size()))
			break;
	}
	return packages;
}
