#pragma once

#include <string>
#include <string_view>
#include <vector>

// a problem the checks found in an INF's text: where (the text it is about), how bad, and what
struct InfProblem {
	size_t Start, End;		// in the text (UTF-8)
	bool Error;				// else a warning
	std::string Message;	// UTF-8
};

//
// What can be told of an INF from its text alone, as it is typed (InfVerif tells more, and takes its time):
// - a string key (%key%) not defined in [Strings] or a localized [Strings.xxxx] (a number is a directory ID): an error
// - a models section [Manufacturer] names, or a device's install section (decorated or not), not in the file: an error
// - a section a directive names (CopyFiles, AddReg, AddService...) not in the file: a warning (an included INF may have it;
//   Needs, which always names another INF's, isn't checked)
// - a file to copy (in a section CopyFiles names, or CopyFiles = @file) not in a [SourceDisksFiles] section: an error
// Sorted by where they are.
//
namespace InfChecks {
	std::vector<InfProblem> Check(std::string_view text);
}
