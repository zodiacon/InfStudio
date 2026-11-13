#pragma once

class InfFile;

enum class InfNodeKind {
	Version,
	Manufacturer,		// the [Manufacturer] section
	Models,				// a models section it names
	InstallSection,		// an install section a device in it uses (with its decorations)
	CopyFiles, DelFiles, AddReg, DelReg,	// a section an install section names
	AllSections,
	Section,
};

struct InfNode {
	InfNodeKind Kind;
	std::wstring Text;
	std::vector<InfNode> Children;
};

//
// The tree INF Studio shows of a file: [Version]; [Manufacturer] > its models sections (in the file's order) > the
// install sections their devices use (each once, sorted) > the sections those name (sorted); and all the sections
// (sorted).
//
std::vector<InfNode> AnalyzeInf(InfFile const& inf);
