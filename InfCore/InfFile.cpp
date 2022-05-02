#include "pch.h"
#include "InfFile.h"
#include <cassert>
#include <array>

#pragma comment(lib, "setupapi")

InfFile::~InfFile() {
	Close();
}

bool InfFile::Open(PCWSTR path, PCWSTR cls, UINT* errLine) {
	m_inf = ::SetupOpenInfFile(path, cls, INF_STYLE_WIN4, errLine);
	if (m_inf == INVALID_HANDLE_VALUE)
		return false;
	return Init();
}

bool InfFile::OpenMaster() {
	m_inf = ::SetupOpenMasterInf();
	if (m_inf == INVALID_HANDLE_VALUE)
		return false;
	return Init();
}

void InfFile::Close() {
	if (m_inf != INVALID_HANDLE_VALUE) {
		::SetupCloseInfFile(m_inf);
		m_inf = INVALID_HANDLE_VALUE;
		m_infoBuffer.reset();
		m_info = nullptr;
	}
}

InfFile::operator bool() const {
	return m_inf != INVALID_HANDLE_VALUE;
}

int InfFile::GetFileCount() const {
	return m_info ? m_info->InfCount : -1;
}

std::vector<std::pair<std::wstring, std::wstring>> InfFile::GetVersionSectionInfo(PCWSTR key, UINT fileIndex) const {
	if (!m_info)
		return {};
	DWORD size = 0;
	::SetupQueryInfVersionInformation(m_info, fileIndex, key, nullptr, 0, &size);
	if (size == 0)
		return {};
	auto buffer = std::make_unique<WCHAR[]>(size);
	if (!::SetupQueryInfVersionInformation(m_info, fileIndex, key, buffer.get(), size, nullptr))
		return {};

	std::vector<std::pair<std::wstring, std::wstring>> result;
	for (auto p = buffer.get(); *p; p += wcslen(p) + 1) {
		auto q = p + wcslen(p) + 1;
		result.push_back({ p, q });
		p = q;
	}
	return result;
}

std::vector<std::wstring> InfFile::GetSectionNames() const {
	std::array<WCHAR, 256> name;
	std::vector<std::wstring> names;
	for (UINT i = 0;; i++) {
		if (!::SetupEnumInfSections(m_inf, i, name.data(), (UINT)name.size(), nullptr))
			break;
		names.emplace_back(name.data());
	}
	return names;
}

std::vector<std::pair<std::wstring, std::wstring>> InfFile::GetSectionCompactLines(PCWSTR section) const {
	INFCONTEXT ctx;
	std::vector<std::pair<std::wstring, std::wstring>> lines;
	WCHAR keyname[64];
	if (::SetupFindFirstLine(m_inf, section, nullptr , &ctx)) {
		WCHAR text[256];
		do {
			auto hasKey = ::SetupGetStringField(&ctx, 0, keyname, _countof(keyname), nullptr);
			if (::SetupGetLineText(&ctx, m_inf, section, hasKey ? keyname : nullptr, text, _countof(text), nullptr))
				lines.emplace_back(std::pair{ hasKey ? keyname : L"", text });
		} while (::SetupFindNextLine(&ctx, &ctx));
	}
	return lines;
}

std::vector<std::pair<std::wstring, std::wstring>> InfFile::GetSectionLines(PCWSTR section) const {
	auto count = ::SetupGetLineCount(m_inf, section);
	std::vector<std::pair<std::wstring, std::wstring>> lines;
	lines.reserve(count);
	INFCONTEXT ctx;
	WCHAR text[400], key[128];
	for (int i = 0; i < count; i++) {
		if (::SetupGetLineByIndex(m_inf, section, i, &ctx)) {
			auto hasKey = ::SetupGetStringField(&ctx, 0, key, _countof(key), nullptr);
			if(::SetupGetLineText(&ctx, m_inf, section, nullptr, text, _countof(text), nullptr))
				lines.emplace_back(std::pair{ hasKey ? key : L"", text });
		}
	}
	return lines;
}

bool InfFile::SectionExists(PCWSTR name) const {
	INFCONTEXT ctx;
	return ::SetupFindFirstLine(m_inf, name, nullptr, &ctx);
}

std::vector<std::wstring> InfFile::GetStringPairs(std::wstring const& src, wchar_t ch, wchar_t replaced) {
	// find base string
	auto pos = src.find(ch);
	if (pos == std::wstring::npos)
		return {};

	auto base = src.substr(0, pos);
	pos++;
	std::vector<std::wstring> result;
	while (true) {
		auto pos2 = src.find(ch, pos);
		if (pos2 == std::wstring::npos) {
			result.emplace_back(base + replaced + src.substr(pos));
			break;
		}
		result.emplace_back(base + replaced + src.substr(pos, pos2 - pos));
		pos = pos2 + 1;
	}

	return result;
}

std::wstring InfFile::GetStringValue(PCWSTR section, PCWSTR key, UINT index) const {
	INFCONTEXT ctx;
	std::wstring value;
	if (::SetupFindFirstLine(m_inf, section, key, &ctx)) {
		WCHAR buffer[512];
		if (::SetupGetStringField(&ctx, index, buffer, _countof(buffer), nullptr))
			value = buffer;
	}
	return value;
}

bool InfFile::Init() {
	assert(m_inf != INVALID_HANDLE_VALUE);
	if (m_infoBuffer)
		return true;

	DWORD size = 0;
	::SetupGetInfInformation(m_inf, INFINFO_INF_SPEC_IS_HINF, nullptr, 0, &size);
	if (size == 0)
		return false;

	m_infoBuffer = std::make_unique<BYTE[]>(size);
	m_info = (SP_INF_INFORMATION*)m_infoBuffer.get();
	return ::SetupGetInfInformation(m_inf, INFINFO_INF_SPEC_IS_HINF, m_info, size, nullptr);
}

