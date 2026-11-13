#pragma once

// a section a directive of an install section names: "CopyFiles = Drivers" in [Install.NTamd64]
struct InstallDirective {
	std::wstring Section;		// the install section it is in (with its decorations)
	std::wstring Directive;		// CopyFiles, DelFiles, AddReg or DelReg
	std::wstring Target;		// the section it names
};

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
	// the INF's name before Windows published it (an installed third party's oemNN.inf); empty if it doesn't know one
	std::wstring GetOriginalFileName() const;
	std::vector<std::pair<std::wstring, std::wstring>> GetVersionSectionInfo(PCWSTR key = nullptr, UINT fileIndex = 0) const;
	std::vector<std::wstring> GetSectionNames() const;
	std::vector<std::pair<std::wstring, std::wstring>> GetSectionCompactLines(PCWSTR section) const;
	std::vector<std::pair<std::wstring, std::wstring>> GetSectionLines(PCWSTR section) const;
	std::wstring GetStringValue(PCWSTR section, PCWSTR key, UINT index = 0) const;
	bool SectionExists(PCWSTR name) const;
	// The models sections a Manufacturer line's value names: "Name" names [Name], "Name,NTamd64,NTarm64" names
	// [Name.NTamd64] and [Name.NTarm64]. Blanks around the fields don't count.
	static std::vector<std::wstring> GetModelSections(std::wstring_view value);
	// The sections an install section named in a models section can be: the name with each platform decoration
	// Windows chooses from (none, .NT, .NTamd64, .NTarm64...), each followed by its .HW section.
	static std::vector<std::wstring> InstallSectionNames(std::wstring_view installSection);
	// the CopyFiles, DelFiles, AddReg and DelReg entries of those sections that the file has, in that order
	std::vector<InstallDirective> GetInstallDirectives(std::wstring_view installSection) const;

private:
	bool Init();

	HINF m_inf{ INVALID_HANDLE_VALUE };
	mutable std::unique_ptr<BYTE[]> m_infoBuffer;
	mutable SP_INF_INFORMATION* m_info{ nullptr };
};

