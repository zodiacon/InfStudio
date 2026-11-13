#pragma once

#include <string>
#include <string_view>

// How an INF file's text is stored. The editor always holds UTF-8; this is what the file is converted from and back to.
enum class TextEncoding {
	Ansi,		// no BOM: the ANSI code page, as SetupAPI reads such a file
	Utf8,		// UTF-8 with a BOM
	Utf16,		// UTF-16 LE with a BOM
};

struct TextFile {
	// the file's text as UTF-8, and how it was stored
	static bool Load(PCWSTR path, std::string& utf8, TextEncoding& encoding);
	// writes the text (UTF-8) as encoding, replacing the whole file
	static bool Save(PCWSTR path, std::string_view utf8, TextEncoding encoding);
	// false if the text has characters the ANSI code page can't hold (saving it as ANSI would turn them into '?')
	static bool FitsAnsi(std::string_view utf8);

	static std::string ToUtf8(std::wstring_view text);
	static std::wstring FromUtf8(std::string_view text);
};
