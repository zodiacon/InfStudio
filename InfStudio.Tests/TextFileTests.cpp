#include "TestCommon.h"
#include "TextFile.h"
#include <wil/resource.h>

namespace {
	// UTF-8 of a word with an e acute, a Hebrew word, two Japanese characters and an emoji (a surrogate pair in UTF-16, and in no ANSI code page)
	const std::string Unicode = "Caf\xC3\xA9 \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80";

	std::string Utf16Bytes(std::wstring_view text, bool bom = true) {
		std::string bytes = bom ? "\xFF\xFE" : "";
		bytes.append((const char*)text.data(), text.size() * sizeof(wchar_t));
		return bytes;
	}
}

TEST_CASE("UTF-8 conversions", "[textfile]") {
	CHECK(TextFile::ToUtf8(L"") == "");
	CHECK(TextFile::FromUtf8("") == L"");
	auto wide = TextFile::FromUtf8(Unicode);
	CHECK(wide == L"Caf\u00E9 \u05E9\u05DC\u05D5\u05DD \u65E5\u672C \U0001F600");
	CHECK(TextFile::ToUtf8(wide) == Unicode);
}

TEST_CASE("Loading a UTF-16 file", "[textfile]") {
	TempFile file(L"utf16.inf");
	file.Write(Utf16Bytes(L"[Strings]\r\nA = \"" + TextFile::FromUtf8(Unicode) + L"\"\r\n"));
	std::string text;
	TextEncoding encoding{};
	REQUIRE(TextFile::Load(file.c_str(), text, encoding));
	CHECK(encoding == TextEncoding::Utf16);
	CHECK(text == "[Strings]\r\nA = \"" + Unicode + "\"\r\n");
}

TEST_CASE("Loading a UTF-8 file with a BOM", "[textfile]") {
	TempFile file(L"utf8.inf");
	file.Write("\xEF\xBB\xBF" "A = " + Unicode);
	std::string text;
	TextEncoding encoding{};
	REQUIRE(TextFile::Load(file.c_str(), text, encoding));
	CHECK(encoding == TextEncoding::Utf8);
	CHECK(text == "A = " + Unicode);
}

TEST_CASE("Loading an ANSI file", "[textfile]") {
	TempFile file(L"ansi.inf");
	std::string text;
	TextEncoding encoding{};

	file.Write("[Version]\r\nClass = USB\r\n");
	REQUIRE(TextFile::Load(file.c_str(), text, encoding));
	CHECK(encoding == TextEncoding::Ansi);
	CHECK(text == "[Version]\r\nClass = USB\r\n");

	if (::GetACP() != 1252)
		SKIP("the rest needs code page 1252");
	file.Write("Caf\xE9");
	REQUIRE(TextFile::Load(file.c_str(), text, encoding));
	CHECK(text == "Caf\xC3\xA9");
}

TEST_CASE("Loading empty, odd and missing files", "[textfile]") {
	std::string text = "old";
	TextEncoding encoding{};
	{
		TempFile file(L"empty.inf");
		file.Write("");
		REQUIRE(TextFile::Load(file.c_str(), text, encoding));
		CHECK(encoding == TextEncoding::Ansi);
		CHECK(text.empty());
	}
	{
		// a UTF-16 file with an odd number of bytes: the last byte is dropped
		TempFile file(L"odd.inf");
		file.Write(Utf16Bytes(L"AB") + "x");
		REQUIRE(TextFile::Load(file.c_str(), text, encoding));
		CHECK(encoding == TextEncoding::Utf16);
		CHECK(text == "AB");
	}
	{
		TempFile file(L"BOM only.inf");
		file.Write("\xFF\xFE");
		REQUIRE(TextFile::Load(file.c_str(), text, encoding));
		CHECK(encoding == TextEncoding::Utf16);
		CHECK(text.empty());
	}
	TempFile missing(L"missing.inf");
	CHECK_FALSE(TextFile::Load(missing.c_str(), text, encoding));
}

TEST_CASE("Saving in each encoding", "[textfile]") {
	TempFile file(L"save.inf");
	auto text = "A = " + Unicode + "\r\n";

	REQUIRE(TextFile::Save(file.c_str(), text, TextEncoding::Utf16));
	CHECK(file.Read() == Utf16Bytes(TextFile::FromUtf8(text)));

	REQUIRE(TextFile::Save(file.c_str(), text, TextEncoding::Utf8));
	CHECK(file.Read() == "\xEF\xBB\xBF" + text);

	REQUIRE(TextFile::Save(file.c_str(), "A = 1\r\n", TextEncoding::Ansi));
	CHECK(file.Read() == "A = 1\r\n");
}

TEST_CASE("Saving replaces the whole file", "[textfile]") {
	// (the file used to be opened without truncating, which left the end of a longer old file)
	TempFile file(L"truncate.inf");
	file.Write(std::string(1000, 'x'));
	REQUIRE(TextFile::Save(file.c_str(), "short", TextEncoding::Ansi));
	CHECK(file.Read() == "short");
}

TEST_CASE("Saving and loading again gives the same text", "[textfile]") {
	TempFile file(L"roundtrip.inf");
	auto text = "[Strings]\r\nDesc = \"" + Unicode + "\"\r\n";
	for (auto encoding : { TextEncoding::Utf16, TextEncoding::Utf8 }) {
		REQUIRE(TextFile::Save(file.c_str(), text, encoding));
		std::string loaded;
		TextEncoding loadedEncoding{};
		REQUIRE(TextFile::Load(file.c_str(), loaded, loadedEncoding));
		CHECK(loadedEncoding == encoding);
		CHECK(loaded == text);
	}
}

TEST_CASE("What fits in the ANSI code page", "[textfile]") {
	CHECK(TextFile::FitsAnsi(""));
	CHECK(TextFile::FitsAnsi("[Version]\r\nClass = USB"));
	CHECK_FALSE(TextFile::FitsAnsi("\xF0\x9F\x98\x80"));		// an emoji is in no ANSI code page
	if (::GetACP() == 1252) {
		CHECK(TextFile::FitsAnsi("Caf\xC3\xA9"));
		CHECK_FALSE(TextFile::FitsAnsi("\xD7\xA9"));		// Hebrew
	}
}

TEST_CASE("A save that fails leaves the file as it was", "[textfile]") {
	TempFile file(L"inuse.inf");
	file.Write("the original\r\n");
	{
		// the file open without sharing deletion: it can't be replaced
		wil::unique_hfile open(::CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(open);
		CHECK_FALSE(TextFile::Save(file.c_str(), "the new text", TextEncoding::Ansi));
	}
	CHECK(file.Read() == "the original\r\n");
	// and nothing of the attempt is left next to it
	int left = 0;
	for (auto const& entry : std::filesystem::directory_iterator(file.Path().parent_path()))
		if (entry.path().filename().wstring().find(file.Path().filename().wstring() + L".") == 0)
			left++;
	CHECK(left == 0);

	// a folder that isn't there
	auto nowhere = file.Path().parent_path() / L"no such folder" / L"x.inf";
	CHECK_FALSE(TextFile::Save(nowhere.c_str(), "x", TextEncoding::Ansi));
}

TEST_CASE("Saving keeps the file's attributes", "[textfile]") {
	TempFile file(L"hidden.inf");
	file.Write("old");
	REQUIRE(::SetFileAttributes(file.c_str(), FILE_ATTRIBUTE_HIDDEN));
	REQUIRE(TextFile::Save(file.c_str(), "new", TextEncoding::Ansi));
	CHECK(file.Read() == "new");
	auto attributes = ::GetFileAttributes(file.c_str());
	CHECK((attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
	CHECK((attributes & FILE_ATTRIBUTE_TEMPORARY) == 0);
	::SetFileAttributes(file.c_str(), FILE_ATTRIBUTE_NORMAL);

	// a new file
	TempFile created(L"created.inf");
	REQUIRE(TextFile::Save(created.c_str(), "fresh", TextEncoding::Ansi));
	CHECK(created.Read() == "fresh");
	CHECK((::GetFileAttributes(created.c_str()) & FILE_ATTRIBUTE_TEMPORARY) == 0);
}

TEST_CASE("Which files open read-only", "[textfile]") {
	// a file of one's own
	TempFile mine(L"mine.inf");
	mine.Write("x");
	CHECK_FALSE(TextFile::ShouldOpenReadOnly(mine.c_str()));
	// marked read-only
	REQUIRE(::SetFileAttributes(mine.c_str(), FILE_ATTRIBUTE_READONLY));
	CHECK(TextFile::ShouldOpenReadOnly(mine.c_str()));
	::SetFileAttributes(mine.c_str(), FILE_ATTRIBUTE_NORMAL);
	// Windows' (even if it could be written: as an administrator)
	WCHAR windows[MAX_PATH];
	REQUIRE(::GetWindowsDirectory(windows, MAX_PATH));
	auto inf = std::filesystem::path(windows) / L"INF" / L"usbport.inf";
	CHECK(TextFile::ShouldOpenReadOnly(inf.c_str()));
	CHECK(TextFile::ShouldOpenReadOnly((std::filesystem::path(windows) / L"System32" / L"DriverStore" / L"x.inf").c_str()));
	// (a folder whose name only starts like Windows' isn't in it)
	CHECK_FALSE(TextFile::IsUnder(std::filesystem::path(windows).wstring() + L"Old" + L"/x.inf", windows));
	CHECK(TextFile::IsUnder(std::filesystem::path(windows).wstring() + L"/INF/x.inf", windows));
}

TEST_CASE("A file's stamp tells whether it changed", "[textfile]") {
	TempFile file(L"stamped.inf");
	file.Write("one");
	auto before = TextFile::Stamp(file.c_str());
	REQUIRE(before);
	CHECK(TextFile::Stamp(file.c_str()) == before);

	// other contents (another size)
	file.Write("other contents");
	auto after = TextFile::Stamp(file.c_str());
	REQUIRE(after);
	CHECK(after != before);

	// the same size, another time
	wil::unique_hfile handle(::CreateFile(file.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
	REQUIRE(handle);
	FILETIME time{ 0x12345678, 0x01D00000 };
	REQUIRE(::SetFileTime(handle.get(), nullptr, nullptr, &time));
	handle.reset();
	CHECK(TextFile::Stamp(file.c_str()) != after);

	// not there
	TempFile missing(L"no-stamp.inf");
	CHECK_FALSE(TextFile::Stamp(missing.c_str()));
}
