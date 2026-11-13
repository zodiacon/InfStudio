#include "pch.h"
#include "InfFile.h"
#include <cassert>
#include <array>
#include <algorithm>

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

namespace {
	std::wstring_view Trim(std::wstring_view text) noexcept {
		constexpr PCWSTR blanks = L" \t";
		auto first = text.find_first_not_of(blanks);
		if (first == std::wstring_view::npos)
			return {};
		return text.substr(first, text.find_last_not_of(blanks) - first + 1);
	}

	// the comma separated fields of a value, trimmed (empty ones included)
	std::vector<std::wstring_view> Fields(std::wstring_view value) {
		std::vector<std::wstring_view> fields;
		for (size_t pos = 0;;) {
			auto comma = value.find(L',', pos);
			fields.push_back(Trim(value.substr(pos, comma == std::wstring_view::npos ? std::wstring_view::npos : comma - pos)));
			if (comma == std::wstring_view::npos)
				break;
			pos = comma + 1;
		}
		return fields;
	}
}

std::vector<std::wstring> InfFile::InstallSectionNames(std::wstring_view installSection) {
	auto name = Trim(installSection);
	if (name.empty())
		return {};
	std::vector<std::wstring> names;
	for (auto decoration : { L"", L".NT", L".NTamd64", L".NTarm64", L".NTx86", L".NTarm", L".NTia64" }) {
		auto section = std::wstring(name) + decoration;
		names.push_back(section);
		names.push_back(section + L".HW");
	}
	return names;
}

std::vector<InstallDirective> InfFile::GetInstallDirectives(std::wstring_view installSection) const {
	std::vector<InstallDirective> directives;
	for (auto const& section : InstallSectionNames(installSection)) {
		for (auto directive : { L"CopyFiles", L"DelFiles", L"AddReg", L"DelReg" }) {
			for (auto const& [key, value] : GetSectionCompactLines(section.c_str())) {
				if (_wcsicmp(key.c_str(), directive) != 0)
					continue;
				for (auto target : Fields(value)) {
					auto exists = [&](InstallDirective const& d) {
						return d.Section == section && d.Directive == directive && _wcsicmp(d.Target.c_str(), std::wstring(target).c_str()) == 0;
					};
					// (empty fields name nothing; a target named twice counts once)
					if (!target.empty() && std::none_of(directives.begin(), directives.end(), exists))
						directives.push_back({ section, directive, std::wstring(target) });
				}
			}
		}
	}
	return directives;
}

std::vector<std::wstring> InfFile::GetModelSections(std::wstring_view value) {
	auto fields = Fields(value);
	auto name = fields[0];
	if (name.empty())
		return {};

	std::vector<std::wstring> sections;
	for (size_t i = 1; i < fields.size(); i++)
		if (!fields[i].empty())
			sections.push_back(std::wstring(name) + L'.' + std::wstring(fields[i]));
	// no decorations: the name is the section
	if (sections.empty())
		sections.emplace_back(name);
	return sections;
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

