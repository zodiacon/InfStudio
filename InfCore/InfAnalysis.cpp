#include "pch.h"
#include "InfAnalysis.h"
#include "InfFile.h"

#include <algorithm>

namespace {
	void SortByText(std::vector<InfNode>& nodes) {
		std::sort(nodes.begin(), nodes.end(), [](auto const& a, auto const& b) { return _wcsicmp(a.Text.c_str(), b.Text.c_str()) < 0; });
	}

	InfNode& ChildNamed(InfNode& parent, std::wstring const& text, InfNodeKind kind) {
		for (auto& child : parent.Children)
			if (_wcsicmp(child.Text.c_str(), text.c_str()) == 0)
				return child;
		return parent.Children.emplace_back(InfNode{ kind, text });
	}

	InfNodeKind DirectiveKind(std::wstring const& directive) {
		if (directive == L"CopyFiles")
			return InfNodeKind::CopyFiles;
		if (directive == L"DelFiles")
			return InfNodeKind::DelFiles;
		if (directive == L"AddReg")
			return InfNodeKind::AddReg;
		return InfNodeKind::DelReg;
	}
}

std::vector<InfNode> AnalyzeInf(InfFile const& inf) {
	std::vector<InfNode> nodes;
	if (inf.SectionExists(L"Version"))
		nodes.push_back({ InfNodeKind::Version, L"Version" });

	auto manufacturers = inf.GetSectionCompactLines(L"Manufacturer");
	if (!manufacturers.empty()) {
		InfNode manufacturer{ InfNodeKind::Manufacturer, L"Manufacturer" };
		for (auto const& [key, value] : manufacturers) {
			for (auto const& section : InfFile::GetModelSections(value)) {
				auto& models = ChildNamed(manufacturer, section, InfNodeKind::Models);
				// each device's install section is the first field of its value
				for (auto const& [device, install] : inf.GetSectionCompactLines(section.c_str())) {
					for (auto const& d : inf.GetInstallDirectives(install.substr(0, install.find(L',')))) {
						auto& installSection = ChildNamed(models, d.Section, InfNodeKind::InstallSection);
						ChildNamed(installSection, d.Target, DirectiveKind(d.Directive));
					}
				}
				SortByText(models.Children);
				for (auto& installSection : models.Children)
					SortByText(installSection.Children);
			}
		}
		nodes.push_back(std::move(manufacturer));
	}

	InfNode all{ InfNodeKind::AllSections, L"All Sections" };
	for (auto const& name : inf.GetSectionNames())
		all.Children.push_back({ InfNodeKind::Section, name });
	SortByText(all.Children);
	nodes.push_back(std::move(all));
	return nodes;
}
