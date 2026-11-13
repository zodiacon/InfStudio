#include "TestCommon.h"
#include "InfVerif.h"

using Severity = InfVerifMessage::Severity;

TEST_CASE("Reading InfVerif's output", "[infverif]") {
	// (lines in infverif.exe 10.0.28000's format, and lines that aren't messages)
	auto messages = InfVerif::Parse(
		"ERROR(1000) in Temp\\ivt\\bad.inf, line 0: N\r\n"
		"ERROR(1233) in C:\\Temp\\ivt\\bad.inf, line 1: Missing directive CatalogFile required for digital signature.\r\n"
		"WARNING(1324) in C:\\Temp\\ivt\\bad.inf, line 1: [Version] section should specify PnpLockdown=1.\r\n"
		"ERROR(1235) in C:\\A, B\\bad.inf, line 10: String token 'Dev' not defined in [Strings] section.\r\n"
		"INFO(2000) in C:\\Temp\\bad.inf, line 3: Something to know: with a colon.\n"
		"\r\n"
		"Checked 1 INF(s) in 0 m 0 s 120 ms\r\n"
		"NOTE(1) in x, line 1: not a severity\r\n"
		"ERROR(x) in y, line 1: not a code\r\n"
		"ERROR(5) in y, line z: not a line\r\n"
		"ERROR(5) in y without a line\r\n");
	REQUIRE(messages.size() == 5);

	CHECK(messages[0].Kind == Severity::Error);
	CHECK(messages[0].Code == 1000);
	CHECK(messages[0].Line == 0);
	CHECK(messages[0].Text == L"N");

	CHECK(messages[1].Code == 1233);
	CHECK(messages[1].Line == 1);
	CHECK(messages[1].Text == L"Missing directive CatalogFile required for digital signature.");

	CHECK(messages[2].Kind == Severity::Warning);
	CHECK(messages[2].Code == 1324);

	// a comma in the path
	CHECK(messages[3].Line == 10);
	CHECK(messages[3].Text == L"String token 'Dev' not defined in [Strings] section.");

	// a colon in the text, and LF alone
	CHECK(messages[4].Kind == Severity::Info);
	CHECK(messages[4].Line == 3);
	CHECK(messages[4].Text == L"Something to know: with a colon.");

	CHECK(InfVerif::Parse("").empty());
	CHECK(InfVerif::Parse("Checked 1 INF(s)").empty());
}

TEST_CASE("Running InfVerif", "[infverif]") {
	auto infverif = InfVerif::Find();
	if (infverif.empty())
		SKIP("the WDK's infverif.exe isn't installed");

	TempFile file(L"flawed.inf");
	file.Write(
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"Class = System\r\n"
		"ClassGuid = {4d36e97d-e325-11ce-bfc1-08002be10318}\r\n"
		"Provider = %Mfg%\r\n"
		"DriverVer = 01/01/2026,1.0.0.0\r\n"
		"[Manufacturer]\r\n"
		"%Mfg% = Contoso,NTamd64\r\n"
		"[Contoso.NTamd64]\r\n"
		"%Dev% = Install, PCI\\VEN_1234&DEV_5678\r\n"
		"[Install.NT]\r\n"
		"[Strings]\r\n"
		"Mfg = \"Contoso\"\r\n");
	auto result = InfVerif::Run(infverif, L"/w", file.Path());
	CAPTURE(result.Output);
	REQUIRE(result.Ran);
	CHECK(result.ExitCode != 0);
	auto has = [&](int code, int line) {
		return std::any_of(result.Messages.begin(), result.Messages.end(), [&](auto const& m) {
			return m.Code == code && m.Line == line && m.Kind == Severity::Error;
		});
	};
	CHECK(has(1233, 1));		// no CatalogFile
	CHECK(has(1235, 10));		// %Dev% not in [Strings]
}
