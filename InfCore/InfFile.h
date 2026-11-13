#pragma once

class InfFile {
public:
	~InfFile();

	bool Open(PCWSTR path, PCWSTR cls = nullptr, UINT* errorLine = nullptr);
	bool OpenMaster();
	void Close();
	bool IsValid() const {
		return (bool)*this;
	}
	operator bool() const;

	int GetFileCount() const;
	std::vector<std::pair<std::wstring, std::wstring>> GetVersionSectionInfo(PCWSTR key = nullptr, UINT fileIndex = 0) const;
	std::vector<std::wstring> GetSectionNames() const;
	std::vector<std::pair<std::wstring, std::wstring>> GetSectionCompactLines(PCWSTR section) const;
	std::vector<std::pair<std::wstring, std::wstring>> GetSectionLines(PCWSTR section) const;
	std::wstring GetStringValue(PCWSTR section, PCWSTR key, UINT index = 0) const;
	bool SectionExists(PCWSTR name) const;
	// The models sections a Manufacturer line's value names: "Name" names [Name], "Name,NTamd64,NTarm64" names
	// [Name.NTamd64] and [Name.NTarm64]. Blanks around the fields don't count.
	static std::vector<std::wstring> GetModelSections(std::wstring_view value);

private:
	bool Init();

	HINF m_inf{ INVALID_HANDLE_VALUE };
	mutable std::unique_ptr<BYTE[]> m_infoBuffer;
	mutable SP_INF_INFORMATION* m_info{ nullptr };
};

