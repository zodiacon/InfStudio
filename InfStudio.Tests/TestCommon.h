#pragma once

#define NOMINMAX
#include <Windows.h>
#include <SetupAPI.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>

#include <scintilla/ILexer.h>
#include <catch2/catch_test_macros.hpp>

// a file in the temporary directory, deleted when the test is done with it
class TempFile {
public:
	explicit TempFile(std::wstring_view name)
		: m_path(std::filesystem::temp_directory_path() / (L"InfStudioTests-" + std::to_wstring(::GetCurrentProcessId()) + L"-" + std::wstring(name))) {
	}
	~TempFile() {
		std::error_code ec;
		std::filesystem::remove(m_path, ec);
	}
	TempFile(TempFile const&) = delete;
	TempFile& operator=(TempFile const&) = delete;

	std::filesystem::path const& Path() const noexcept {
		return m_path;
	}
	PCWSTR c_str() const noexcept {
		return m_path.c_str();
	}

	void Write(std::string_view bytes) const {
		std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), bytes.size());
	}
	std::string Read() const {
		std::ifstream in(m_path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}

private:
	std::filesystem::path m_path;
};
