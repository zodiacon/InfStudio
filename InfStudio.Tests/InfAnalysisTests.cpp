#include "TestCommon.h"
#include "InfFile.h"
#include "InfAnalysis.h"

namespace {
	// the tree as text: one node a line, indented two spaces a level, its kind in brackets
	PCWSTR KindName(InfNodeKind kind) {
		switch (kind) {
			case InfNodeKind::Version: return L"version";
			case InfNodeKind::Manufacturer: return L"manufacturer";
			case InfNodeKind::Models: return L"models";
			case InfNodeKind::InstallSection: return L"install";
			case InfNodeKind::CopyFiles: return L"copyfiles";
			case InfNodeKind::DelFiles: return L"delfiles";
			case InfNodeKind::AddReg: return L"addreg";
			case InfNodeKind::DelReg: return L"delreg";
			case InfNodeKind::AllSections: return L"sections";
			case InfNodeKind::Section: return L"section";
		}
		return L"?";
	}

	void Dump(std::vector<InfNode> const& nodes, std::wstring& text, int depth = 0) {
		for (auto const& node : nodes) {
			text += std::wstring(depth * 2, L' ') + node.Text + L" [" + KindName(node.Kind) + L"]\n";
			Dump(node.Children, text, depth + 1);
		}
	}

	std::wstring Analyze(std::string_view inf) {
		TempFile file(L"analysis.inf");
		file.Write(inf);
		InfFile infFile;
		REQUIRE(infFile.Open(file.c_str()));
		std::wstring text;
		Dump(AnalyzeInf(infFile), text);
		return text;
	}
}

TEST_CASE("The tree of an INF", "[analysis]") {
	auto tree = Analyze(
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"[Manufacturer]\r\n"
		"%A% = Plain\r\n"
		"%B% = Contoso, NTamd64\r\n"
		"[Plain]\r\n"
		"%D% = Plain_Install, ID1\r\n"
		"[Contoso.NTamd64]\r\n"
		"%D% = Install, ID2\r\n"
		"%E% = Install, ID3\r\n"
		"[Plain_Install]\r\n"
		"CopyFiles = Files\r\n"
		"[Install.NTamd64]\r\n"
		"CopyFiles = Zeta.Files, Alpha.Files\r\n"
		"DelReg = Old.Reg\r\n"
		"[Install.NTamd64.HW]\r\n"
		"AddReg = Hw.Reg\r\n"
		"[Strings]\r\n"
		"A = \"A\"\r\n"
		"B = \"B\"\r\n"
		"D = \"D\"\r\n"
		"E = \"E\"\r\n");
	CHECK(tree ==
		L"Version [version]\n"
		L"Manufacturer [manufacturer]\n"
		L"  Plain [models]\n"
		L"    Plain_Install [install]\n"
		L"      Files [copyfiles]\n"
		L"  Contoso.NTamd64 [models]\n"
		// (two devices with the same install section: it's there once; the sections and their targets sorted)
		L"    Install.NTamd64 [install]\n"
		L"      Alpha.Files [copyfiles]\n"
		L"      Old.Reg [delreg]\n"
		L"      Zeta.Files [copyfiles]\n"
		L"    Install.NTamd64.HW [install]\n"
		L"      Hw.Reg [addreg]\n"
		L"All Sections [sections]\n"
		L"  contoso.ntamd64 [section]\n"
		L"  install.ntamd64 [section]\n"
		L"  install.ntamd64.hw [section]\n"
		L"  manufacturer [section]\n"
		L"  plain [section]\n"
		L"  plain_install [section]\n"
		L"  strings [section]\n"
		L"  version [section]\n");
}

TEST_CASE("The tree of an INF without devices", "[analysis]") {
	auto tree = Analyze(
		"[Version]\r\n"
		"Signature = \"$Windows NT$\"\r\n"
		"[DefaultInstall.NTamd64]\r\n"
		"CopyFiles = Files\r\n");
	CHECK(tree ==
		L"Version [version]\n"
		L"All Sections [sections]\n"
		L"  defaultinstall.ntamd64 [section]\n"
		L"  version [section]\n");
}
