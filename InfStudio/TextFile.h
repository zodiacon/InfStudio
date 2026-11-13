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

	// A file to open read-only: Windows' (under its folder, as %windir%\INF and the driver store are: not to be edited even
	// by an administrator), marked read-only, or one that can't be opened for writing.
	static bool ShouldOpenReadOnly(PCWSTR path);
	// whether path is in folder (or deeper), by whole names, whatever their case
	static bool IsUnder(std::wstring_view path, std::wstring_view folder);

	static std::string ToUtf8(std::wstring_view text);
	static std::wstring FromUtf8(std::string_view text);
};
