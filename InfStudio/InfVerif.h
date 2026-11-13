#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

//
// The WDK's INF verifier (infverif.exe): finding it, running it on a file, and reading what it reports.
//
struct InfVerifMessage {
	enum class Severity { Error, Warning, Info };
	Severity Kind;
	int Code;
	int Line;				// 1-based; 0 for the file as a whole
	std::wstring Text;
};

struct InfVerifResult {
	bool Ran{ false };		// false if infverif.exe couldn't be started
	DWORD ExitCode{ 0 };
	std::string Output;
	std::vector<InfVerifMessage> Messages;
};

namespace InfVerif {
	// the newest WDK's infverif.exe for this machine's architecture, or empty
	std::filesystem::path Find();
	// mode: "/w" (Windows Driver requirements), "/h" (WHQL), "/u" (Universal)...
	InfVerifResult Run(std::filesystem::path const& infverif, PCWSTR mode, std::filesystem::path const& inf);
	// the "ERROR(1233) in <path>, line 12: <text>" lines of its output (and WARNING, INFO)
	std::vector<InfVerifMessage> Parse(std::string_view output);
}
