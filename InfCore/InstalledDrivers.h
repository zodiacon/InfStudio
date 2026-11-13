#pragma once

#include <filesystem>
#include <functional>

// an INF Windows has (in %windir%\INF): a driver package
struct DriverPackage {
	std::wstring PublishedName;		// its name in %windir%\INF: oem12.inf, usbport.inf
	std::wstring OriginalName;		// a third party's: the INF's own name (empty if it's the published one)
	std::wstring Provider, Class, ClassGuid;
	std::wstring Date, Version;		// of DriverVer; the date as yyyy-mm-dd (so that it sorts)
	std::wstring Path;				// in %windir%\INF
	std::wstring StorePath;			// its copy in the driver store (FileRepository), if it has one
	bool ThirdParty{ false };		// oemNN.inf
	bool Valid{ false };			// SetupAPI could open it
};

namespace InstalledDrivers {
	// %windir%\INF
	std::filesystem::path InfDirectory();
	// a package's details (SetupAPI: strings replaced)
	DriverPackage Read(std::filesystem::path const& inf);
	// The *.inf of a folder, sorted by name. progress(done, count) is called after each one; false stops (what was read
	// so far is returned).
	std::vector<DriverPackage> ReadAll(std::filesystem::path const& dir, std::function<bool(size_t, size_t)> const& progress);

	// a name Windows publishes a third party's INF by: oem<number>.inf
	bool IsThirdParty(std::wstring_view name);
	// DriverVer's "mm/dd/yyyy,version" as (yyyy-mm-dd, version); a date that isn't one as it is
	std::pair<std::wstring, std::wstring> SplitDriverVer(std::wstring_view driverVer);
}
