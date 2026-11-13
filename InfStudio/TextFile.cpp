#include "pch.h"
#include "TextFile.h"

namespace {
	std::string WideToMultiByte(UINT codePage, std::wstring_view text, BOOL* usedDefault = nullptr) {
		if (text.empty())
			return {};
		// (CP_UTF8 takes no default character)
		auto flags = codePage == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS;
		auto pDefault = codePage == CP_UTF8 ? nullptr : usedDefault;
		int size = ::WideCharToMultiByte(codePage, flags, text.data(), (int)text.size(), nullptr, 0, nullptr, pDefault);
		std::string result(size, '\0');
		::WideCharToMultiByte(codePage, flags, text.data(), (int)text.size(), result.data(), size, nullptr, pDefault);
		return result;
	}

	std::wstring MultiByteToWide(UINT codePage, std::string_view text) {
		if (text.empty())
			return {};
		int size = ::MultiByteToWideChar(codePage, 0, text.data(), (int)text.size(), nullptr, 0);
		std::wstring result(size, L'\0');
		::MultiByteToWideChar(codePage, 0, text.data(), (int)text.size(), result.data(), size);
		return result;
	}

	constexpr char Utf8Bom[] = "\xEF\xBB\xBF";
}

std::string TextFile::ToUtf8(std::wstring_view text) {
	return WideToMultiByte(CP_UTF8, text);
}

std::wstring TextFile::FromUtf8(std::string_view text) {
	return MultiByteToWide(CP_UTF8, text);
}

bool TextFile::Load(PCWSTR path, std::string& utf8, TextEncoding& encoding) {
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
	if (!hFile)
		return false;

	LARGE_INTEGER size;
	if (!::GetFileSizeEx(hFile.get(), &size) || size.QuadPart > 256 << 20)
		return false;

	std::string bytes((size_t)size.QuadPart, '\0');
	DWORD read = 0;
	if (!bytes.empty() && (!::ReadFile(hFile.get(), bytes.data(), (DWORD)bytes.size(), &read, nullptr) || read != bytes.size()))
		return false;

	if (bytes.size() >= 2 && (BYTE)bytes[0] == 0xFF && (BYTE)bytes[1] == 0xFE) {
		encoding = TextEncoding::Utf16;
		utf8 = ToUtf8(std::wstring_view((PCWSTR)(bytes.data() + 2), (bytes.size() - 2) / sizeof(WCHAR)));
	}
	else if (bytes.starts_with(Utf8Bom)) {
		encoding = TextEncoding::Utf8;
		utf8 = bytes.substr(3);
	}
	else {
		encoding = TextEncoding::Ansi;
		utf8 = ToUtf8(MultiByteToWide(CP_ACP, bytes));
	}
	return true;
}

bool TextFile::Save(PCWSTR path, std::string_view utf8, TextEncoding encoding) {
	std::string bytes;
	switch (encoding) {
		case TextEncoding::Utf16:
		{
			auto text = FromUtf8(utf8);
			bytes = "\xFF\xFE";
			bytes.append((const char*)text.data(), text.size() * sizeof(WCHAR));
			break;
		}
		case TextEncoding::Utf8:
			bytes = Utf8Bom;
			bytes += utf8;
			break;

		default:
			bytes = WideToMultiByte(CP_ACP, FromUtf8(utf8));
			break;
	}

	// CREATE_ALWAYS: the old contents go, even if longer than the new
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return false;

	DWORD written = 0;
	return ::WriteFile(hFile.get(), bytes.data(), (DWORD)bytes.size(), &written, nullptr) && written == bytes.size();
}

bool TextFile::FitsAnsi(std::string_view utf8) {
	BOOL usedDefault = FALSE;
	WideToMultiByte(CP_ACP, FromUtf8(utf8), &usedDefault);
	return !usedDefault;
}
