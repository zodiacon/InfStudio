#include "TestCommon.h"
#include "InfChecks.h"
#include "InfTemplates.h"

#include <algorithm>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace {
	struct Found {
		std::string Text;		// what the problem underlines
		bool Error;
		std::string Message;
	};

	std::vector<Found> Check(std::string const& text) {
		std::vector<Found> found;
		for (auto const& p : InfChecks::Check(text))
			found.push_back({ text.substr(p.Start, p.End - p.Start), p.Error, p.Message });
		return found;
	}

	std::vector<std::string> Underlined(std::vector<Found> const& found) {
		std::vector<std::string> texts;
		for (auto const& f : found)
			texts.push_back(f.Text);
		return texts;
	}

	using Texts = std::vector<std::string>;
}

TEST_CASE("String keys not in [Strings]", "[checks]") {
	auto found = Check(
		"[Version]\r\n"
		"Provider = %Mfg%\r\n"
		"Class = %Nope% ; %AlsoNope% in a comment\r\n"
		"ServiceBinary = %13%\\a.sys\r\n"
		"Desc = \"%Local% and %Nope2%\"\r\n"
		"[Strings]\r\n"
		"Mfg = \"x\"\r\n"
		"[Strings.0407]\r\n"
		"Local = \"y\"\r\n");
	// (a number is a directory ID; a key defined only in a localized [Strings] is defined)
	CHECK(Underlined(found) == Texts{ "%Nope%", "%Nope2%" });
	REQUIRE(found.size() == 2);
	CHECK(found[0].Error);
	CHECK(found[0].Message.find("Nope") != std::string::npos);

	// no [Strings] at all
	CHECK(Underlined(Check("[Version]\r\nProvider = %Mfg%\r\n")) == Texts{ "%Mfg%" });
}

TEST_CASE("Sections that aren't in the file", "[checks]") {
	// a models section [Manufacturer] names, decorated or not: an error
	auto models = Check("[Manufacturer]\r\nA = Contoso, NTamd64\r\nB = Plain\r\n[Contoso.NTamd64]\r\n");
	CHECK(Underlined(models) == Texts{ "Plain" });
	REQUIRE(models.size() == 1);
	CHECK(models[0].Error);
	CHECK(Underlined(Check("[Manufacturer]\r\nA = Contoso, NTamd64\r\n")) == Texts{ "Contoso, NTamd64" });

	// a device's install section: there if it or a decorated one is
	const std::string devices = "[Manufacturer]\r\nA = Contoso\r\n[Contoso]\r\nD1 = Inst, ID1\r\nD2 = Gone, ID2\r\n";
	auto installs = Check(devices + "[Inst.NT]\r\n");
	CHECK(Underlined(installs) == Texts{ "Gone" });
	REQUIRE(installs.size() == 1);
	CHECK(installs[0].Error);

	// what a directive names: a warning (an included INF may have it); Needs names another INF's
	auto lists = Check(devices + "[Inst.NT]\r\nCopyFiles = Files\r\nAddReg = Reg, Here.Reg\r\nNeeds = WINUSB.NT\r\n"
		"[Inst.NT.Services]\r\nAddService = Drv, 2, Svc\r\n[Here.Reg]\r\n[Gone]\r\n");
	CHECK(Underlined(lists) == Texts{ "Files", "Reg", "Svc" });
	for (auto const& f : lists)
		CHECK_FALSE(f.Error);
}

TEST_CASE("Files to copy that aren't in [SourceDisksFiles]", "[checks]") {
	auto found = Check(
		"[DefaultInstall.NTamd64]\r\n"
		"CopyFiles = Files, @single.sys, @nowhere.sys\r\n"
		"DelFiles = Old\r\n"
		"[Files]\r\n"
		"a.sys\r\n"
		"b.sys, src_b.sys\r\n"
		"c.sys ; not there\r\n"
		"%FileName%\r\n"
		"\r\n"
		"[Old]\r\n"
		"gone.sys\r\n"
		"[SourceDisksFiles]\r\n"
		"a.sys = 1\r\n"
		"src_b.sys = 1\r\n"
		"[SourceDisksFiles.amd64]\r\n"
		"single.sys = 1\r\n"
		"[Strings]\r\n"
		"FileName = \"d.sys\"\r\n");
	// (b.sys comes from src_b.sys; a name a string key gives isn't known; DelFiles deletes, it copies nothing)
	CHECK(Underlined(found) == Texts{ "@nowhere.sys", "c.sys" });
	for (auto const& f : found)
		CHECK(f.Error);
}

TEST_CASE("Nothing to report", "[checks]") {
	CHECK(InfChecks::Check("").empty());
	CHECK(InfChecks::Check("; just a comment\r\n[Version]\r\nSignature = \"$Windows NT$\"\r\n").empty());
}

TEST_CASE("The templates have no problems", "[checks][templates]") {
	auto index = GENERATE(range(size_t(0), InfTemplates::All().size()));
	auto const& t = InfTemplates::All()[index];
	auto text = InfTemplates::Generate(t, InfTemplates::NewValues());
	auto found = Check(text);
	for (auto const& f : found)
		UNSCOPED_INFO(f.Text << ": " << f.Message);
	CHECK(found.empty());
}
