#include "TestCommon.h"
#include "InfCompletion.h"

#include <algorithm>

namespace {
	// the completion at the '|' in text (which is taken out)
	std::optional<InfCompletion::List> At(std::string text, size_t* caret = nullptr) {
		auto pos = text.find('|');
		REQUIRE(pos != std::string::npos);
		text.erase(pos, 1);
		if (caret)
			*caret = pos;
		return InfCompletion::Complete(text, pos);
	}

	bool Has(std::optional<InfCompletion::List> const& list, std::string_view item) {
		return list && std::find(list->Items.begin(), list->Items.end(), item) != list->Items.end();
	}

	using Items = std::vector<std::string>;
}

TEST_CASE("Completing a string key", "[completion]") {
	const std::string strings = "[Strings]\r\nMfg = \"C\"\r\nDev = \"D\"\r\n[Strings.0407]\r\nOnly = \"x\"\r\nmfg = \"y\"\r\n";
	size_t caret;
	auto list = At("[Version]\r\nProvider = %|\r\n" + strings, &caret);
	REQUIRE(list);
	// each key once (whatever its case), from [Strings] and the localized ones, sorted
	CHECK(list->Items == Items{ "Dev", "Mfg", "Only" });
	CHECK(list->Start == caret);
	CHECK(list->Suffix == "%");

	// a part typed: from it; the closing percent there already: none added
	list = At("[Version]\r\nProvider = %Mf|\r\n" + strings, &caret);
	REQUIRE(list);
	CHECK(list->Start == caret - 2);
	CHECK(list->Suffix == "%");
	list = At("[Version]\r\nProvider = %Mf|%\r\n" + strings);
	REQUIRE(list);
	CHECK(list->Suffix.empty());

	// in a string, and as a key
	CHECK(Has(At("[Version]\r\nA = \"%|\"\r\n" + strings), "Mfg"));
	CHECK(Has(At("[Manufacturer]\r\n%|\r\n" + strings), "Mfg"));

	// not after a whole key, a literal %%, or in a comment
	CHECK_FALSE(At("[Version]\r\nProvider = %Mfg%|\r\n" + strings));
	CHECK_FALSE(At("[Version]\r\nA = 100%%|\r\n" + strings));
	CHECK_FALSE(At("[Version]\r\nA = 1 ; %|\r\n" + strings));
}

TEST_CASE("Completing a section header", "[completion]") {
	size_t caret;
	auto list = At(
		"[Version]\r\n"
		"[Manufacturer]\r\n"
		"%M% = Contoso,NTamd64\r\n"
		"[Contoso.NTamd64]\r\n"
		"%D% = Install, ID\r\n"
		"%E% = Other, ID2\r\n"
		"[Install.NT]\r\n"
		"CopyFiles = Files, @single.sys\r\n"
		"[Install.NT.Services]\r\n"
		"AddService = Drv, 2, Svc\r\n"
		"[|", &caret);
	REQUIRE(list);
	CHECK(list->Start == caret);
	CHECK(list->Suffix == "]");
	// referenced and not there yet (an install section is there if a decorated one is), and the standard ones missing
	for (auto item : { "Files", "Svc", "Other", "Strings", "SourceDisksNames", "SourceDisksFiles", "DestinationDirs" })
		CHECK(Has(list, item));
	for (auto item : { "Version", "Manufacturer", "Contoso.NTamd64", "Install", "Install.NT", "@single.sys", "single.sys" })
		CHECK_FALSE(Has(list, item));

	// a part typed; the closing bracket there already
	list = At("[Version]\r\n  [Str|", &caret);
	REQUIRE(list);
	CHECK(list->Start == caret - 3);
	CHECK(Has(list, "Strings"));
	list = At("[Version]\r\n[Str|]");
	REQUIRE(list);
	CHECK(list->Suffix.empty());
	// not after something else on the line
	CHECK_FALSE(At("[Version]\r\nA = [|"));
}

TEST_CASE("Completing a key: what the section's kind takes", "[completion]") {
	size_t caret;
	auto version = At("[Version]\r\nCl|", &caret);
	REQUIRE(version);
	CHECK(version->Start == caret - 2);
	CHECK(version->Suffix == " = ");
	for (auto item : { "Signature", "Class", "ClassGuid", "Provider", "DriverVer", "CatalogFile", "PnpLockdown" })
		CHECK(Has(version, item));
	CHECK_FALSE(Has(version, "CopyFiles"));
	// the '=' there already
	auto withEquals = At("[Version]\r\nCl| = X");
	REQUIRE(withEquals);
	CHECK(withEquals->Suffix.empty());

	const std::string devices = "[Manufacturer]\r\n%M% = Contoso\r\n[Contoso]\r\n%D% = Inst, ID\r\n";
	// an install section a device uses, decorated or not, and DefaultInstall
	for (auto header : { "[Inst]", "[Inst.NT]", "[Inst.NTamd64]", "[DefaultInstall.NTamd64]" }) {
		auto install = At(devices + header + "\r\nCo|");
		CAPTURE(header);
		CHECK(Has(install, "CopyFiles"));
		CHECK(Has(install, "AddReg"));
		CHECK(Has(install, "Include"));
		CHECK_FALSE(Has(install, "AddService"));
	}
	auto services = At(devices + "[Inst.NT.Services]\r\nAdd|");
	CHECK(Has(services, "AddService"));
	CHECK(Has(services, "DelService"));
	CHECK_FALSE(Has(services, "CopyFiles"));
	auto hw = At(devices + "[Inst.NT.HW]\r\nA|");
	CHECK(Has(hw, "AddReg"));
	CHECK(Has(hw, "AddProperty"));
	CHECK_FALSE(Has(hw, "CopyFiles"));
	auto wdf = At(devices + "[Inst.NT.Wdf]\r\nKm|");
	CHECK(Has(wdf, "KmdfService"));
	CHECK(Has(wdf, "UmdfService"));

	// a service section (AddService names it) and a WDF service section (KmdfService names it)
	auto service = At(devices + "[Inst.NT.Services]\r\nAddService = Drv, 2, Svc\r\n[Svc]\r\nSt|");
	CHECK(Has(service, "StartType"));
	CHECK(Has(service, "ServiceBinary"));
	CHECK_FALSE(Has(service, "CopyFiles"));
	auto wdfService = At(devices + "[Inst.NT.Wdf]\r\nKmdfService = Drv, Drv_Wdf\r\n[Drv_Wdf]\r\nKm|");
	CHECK(Has(wdfService, "KmdfLibraryVersion"));

	// [DestinationDirs]: DefaultDestDir and the file sections not listed yet
	auto dirs = At(devices + "[Inst.NT]\r\nCopyFiles = Files, Other\r\nDelFiles = Old\r\n[DestinationDirs]\r\nOther = 13\r\n|");
	REQUIRE(dirs);
	CHECK(dirs->Items == Items{ "DefaultDestDir", "Files", "Old" });

	// sections without keys, unknown ones, before any section, and a value that isn't a section's: nothing
	CHECK_FALSE(At(devices + "[Inst.NT]\r\nCopyFiles = Files\r\n[Files]\r\na|"));
	CHECK_FALSE(At("[Whatever]\r\nX|"));
	CHECK_FALSE(At("Abc|"));
	CHECK_FALSE(At("[Version]\r\nClass = Sy|"));
	// a key after something else on the line
	CHECK_FALSE(At("[Version]\r\n\"x\" Cl|"));
}

TEST_CASE("Completing a value that names sections", "[completion]") {
	const std::string devices = "[Manufacturer]\r\n%M% = Contoso\r\n[Contoso]\r\n%D% = Inst, ID\r\n";
	const std::string sections = "\r\n[Files]\r\na.sys\r\n[Hw.Reg]\r\nHKR,,X,,1\r\n[Inst.NT.HW]\r\n[Svc]\r\nServiceType = 1\r\n[Strings]\r\nM = \"x\"\r\n";
	size_t caret;
	auto list = At(devices + "[Inst.NT]\r\nCopyFiles = |" + sections, &caret);
	REQUIRE(list);
	CHECK(list->Start == caret);
	CHECK(list->Suffix.empty());
	// the sections there that a directive can name: not the install sections, the well-known ones or this one
	CHECK(list->Items == Items{ "Files", "Hw.Reg", "Svc" });

	// a later field, a part typed
	list = At(devices + "[Inst.NT]\r\nAddReg = Hw.Reg, Fi|" + sections, &caret);
	REQUIRE(list);
	CHECK(list->Start == caret - 2);
	// AddService: its third field; KmdfService: its second
	CHECK(Has(At(devices + "[Inst.NT.Services]\r\nAddService = Drv, 2, |" + sections), "Svc"));
	CHECK_FALSE(At(devices + "[Inst.NT.Services]\r\nAddService = Drv, |" + sections));
	CHECK(Has(At(devices + "[Inst.NT.Wdf]\r\nKmdfService = Drv, |" + sections), "Svc"));
	// in a comment
	CHECK_FALSE(At(devices + "[Inst.NT]\r\nCopyFiles = Files ; |" + sections));
}
