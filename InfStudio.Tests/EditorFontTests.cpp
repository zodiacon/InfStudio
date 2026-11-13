#include "TestCommon.h"
#include "EditorFont.h"

namespace {
	LOGFONT Font(PCWSTR face, int height, int weight = FW_NORMAL) {
		LOGFONT lf{};
		wcscpy_s(lf.lfFaceName, face);
		lf.lfHeight = height;
		lf.lfWeight = weight;
		return lf;
	}
}

TEST_CASE("The editor's font until one is chosen", "[font]") {
	auto lf = EditorFont::Normalize(LOGFONT{});
	CHECK(std::wstring(lf.lfFaceName) == L"Consolas");
	CHECK(lf.lfHeight == 110);
	CHECK(lf.lfWeight == FW_NORMAL);
}

TEST_CASE("A saved font's size is kept within bounds", "[font]") {
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", 120)).lfHeight == 120);
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", 0)).lfHeight == EditorFont::DefaultSize);
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", -15)).lfHeight == EditorFont::DefaultSize);
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", 10)).lfHeight == EditorFont::MinSize);
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", 5000)).lfHeight == EditorFont::MaxSize);
	CHECK(std::wstring(EditorFont::Normalize(Font(L"Cascadia Mono", 120, FW_BOLD)).lfFaceName) == L"Cascadia Mono");
	CHECK(EditorFont::Normalize(Font(L"Cascadia Mono", 120, FW_BOLD)).lfWeight == FW_BOLD);
}

TEST_CASE("The font dialog's size and back", "[font]") {
	// 11 points at 96 DPI are 14.67 pixels: 15
	CHECK(EditorFont::ForDialog(Font(L"Consolas", 110), 96).lfHeight == -15);
	CHECK(EditorFont::ForDialog(Font(L"Consolas", 110), 144).lfHeight == -22);
	CHECK(EditorFont::ForDialog(LOGFONT{}, 96).lfHeight == -15);
	// the dialog's size (tenths) is what is saved, whatever its device units
	auto chosen = EditorFont::FromDialog(Font(L"Cascadia Mono", -21), 160);
	CHECK(chosen.lfHeight == 160);
	CHECK(std::wstring(chosen.lfFaceName) == L"Cascadia Mono");
}
