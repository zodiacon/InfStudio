#pragma once

#include <algorithm>

//
// The editor's font (Options > Font), kept in the settings as a LOGFONT whose lfHeight is the size in tenths of a
// point (as AstroStudio keeps its text font): Consolas 11 until the user picks another.
//
struct EditorFont {
	static constexpr int DefaultSize = 110, MinSize = 60, MaxSize = 360;

	// the saved font, or the default if none was chosen; the size within bounds
	static LOGFONT Normalize(LOGFONT lf) noexcept {
		if (lf.lfFaceName[0] == 0) {
			lf = {};
			wcscpy_s(lf.lfFaceName, L"Consolas");
			lf.lfWeight = FW_NORMAL;
			lf.lfHeight = DefaultSize;
		}
		if (lf.lfHeight <= 0)
			lf.lfHeight = DefaultSize;
		lf.lfHeight = std::clamp((int)lf.lfHeight, MinSize, MaxSize);
		return lf;
	}

	// for the font dialog, which wants the height in device units
	static LOGFONT ForDialog(LOGFONT const& saved, int dpi) noexcept {
		auto lf = Normalize(saved);
		lf.lfHeight = -::MulDiv(lf.lfHeight, dpi, 720);
		return lf;
	}

	// what the dialog chose, to save: its size (CFontDialog::GetSize, in tenths of a point) in lfHeight
	static LOGFONT FromDialog(LOGFONT chosen, int sizeTenths) noexcept {
		chosen.lfHeight = sizeTenths;
		return Normalize(chosen);
	}
};
