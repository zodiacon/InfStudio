#include "TestCommon.h"
#include "TextFile.h"

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
