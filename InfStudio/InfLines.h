#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

//
// An INF's text (UTF-8) as lines styled by InfLexer, and what InfNavigation and InfCompletion read from them.
//
namespace InfLines {
	struct StyledLine {
		size_t Start;				// in the text
		std::string_view Text;		// without the line end
		std::vector<uint8_t> Styles;	// InfLexer::Style, one a byte
	};

	// every line of the text (the continuations carried from one to the next, as the lexer does)
	std::vector<StyledLine> StyleLines(std::string_view text);
	// the line pos is on (its end included), or null
	StyledLine const* LineAt(std::vector<StyledLine> const& lines, size_t pos);

	std::string_view Trim(std::string_view text);
	bool EqualNoCase(std::string_view a, std::string_view b);

	// the name of a section header line, if it is one
	std::optional<std::string_view> SectionName(StyledLine const& line);
	// a line's key (before its '='), trimmed; empty if it has none
	std::string_view KeyOf(StyledLine const& line);
	// The fields of a line's value (after its '=', or the whole line if it has none), between its commas, trimmed;
	// to a comment. Offsets are in the line.
	struct Field {
		size_t Start, End;
		std::string_view Text;
	};
	std::vector<Field> ValueFields(StyledLine const& line);

	//
	// What an INF's text has and what its directives name: the sections, the string keys, the files of
	// [SourceDisksFiles], and every reference to a section (where it is, and what kind of section it names).
	//
	using Names = std::vector<std::string_view>;

	// the directives whose every field names a section, those that name file lists, and the sections every INF may have
	extern const Names SectionValueKeys, FileSectionKeys, StandardSections;

	enum class SectionKind { None, Version, DestinationDirs, Install, Hw, Services, Wdf, Service, WdfService, CoInstallers, Interfaces };
	enum class ReferenceKind {
		Models,			// [Manufacturer] names it (with its decoration)
		Install,		// a device in a models section uses it (decorated or not: Install, Install.NT, Install.NTamd64...)
		Service,		// AddService's third field
		WdfService,		// KmdfService's or UmdfService's second
		List,			// CopyFiles, AddReg... (SectionValueKeys; not Needs, whose sections are other INFs')
	};

	struct Reference {
		std::string_view Name;
		size_t Start, End;			// in the text
		ReferenceKind Kind;
		std::string_view Directive;	// the key of the line (CopyFiles...), for a List
	};

	class InfModel {
	public:
		explicit InfModel(std::string_view text);
		InfModel(InfModel const&) = delete;
		InfModel& operator=(InfModel const&) = delete;

		std::vector<StyledLine> Lines;
		std::vector<std::string_view> SectionOfLine;	// the section each line is in (empty before the first)
		Names Defined;					// the sections, in the file's order
		std::vector<Reference> References;
		Names StringKeys;				// of [Strings] and the localized [Strings.xxxx]
		Names SourceFiles;				// the keys of [SourceDisksFiles] (and its decorated forms)

		// the names the references of a kind (and, for List, of a directive) have
		Names Named(ReferenceKind kind, Names const* directives = nullptr) const;
		bool IsDefined(std::string_view name) const;
		// an install section is there if it is, or one of its decorated forms (name.xxx) is
		bool IsInstallDefined(std::string_view name) const;
		SectionKind KindOf(std::string_view section) const;
		// a section a directive's value can name: a list (files, registry...), not one with keys or a well-known one
		bool IsListSection(std::string_view section) const;

	private:
		std::vector<std::unique_ptr<std::string>> m_Composed;	// (decorated models sections' names, which the text doesn't have)
	};
}
