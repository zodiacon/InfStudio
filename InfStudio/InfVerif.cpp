#include "pch.h"
#include "InfVerif.h"

#include <charconv>

namespace {
	std::string_view TrimEnd(std::string_view text) noexcept {
		while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
			text.remove_suffix(1);
		return text;
	}

	bool ParseInt(std::string_view text, int& value) noexcept {
		auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
		return ec == std::errc{} && end == text.data() + text.size();
	}
}

std::filesystem::path InfVerif::Find() {
	SYSTEM_INFO si;
	::GetNativeSystemInfo(&si);
	auto arch = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? L"arm64" : L"x64";

	WCHAR programFiles[MAX_PATH];
	if (!::ExpandEnvironmentStrings(L"%ProgramFiles(x86)%", programFiles, _countof(programFiles)))
		return {};
	std::error_code ec;
	std::filesystem::path found;
	for (auto const& dir : std::filesystem::directory_iterator(std::filesystem::path(programFiles) / L"Windows Kits" / L"10" / L"Tools", ec)) {
		auto exe = dir.path() / arch / L"infverif.exe";
		// (the version folders sort by version as text: 10.0.22621.0 < 10.0.28000.0)
		if (std::filesystem::exists(exe, ec) && (found.empty() || exe > found))
			found = exe;
	}
	return found;
}

InfVerifResult InfVerif::Run(std::filesystem::path const& infverif, PCWSTR mode, std::filesystem::path const& inf, std::stop_token stop) {
	InfVerifResult result;

	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
	wil::unique_handle read, write;
	if (!::CreatePipe(read.addressof(), write.addressof(), &sa, 0))
		return result;
	::SetHandleInformation(read.get(), HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO si{ sizeof(si) };
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = si.hStdError = write.get();
	si.hStdInput = nullptr;
	auto commandLine = std::format(L"\"{}\" {} \"{}\"", infverif.wstring(), mode, inf.wstring());
	PROCESS_INFORMATION pi;
	if (!::CreateProcess(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return result;
	wil::unique_handle process(pi.hProcess), thread(pi.hThread);
	// (the child has its copy: the pipe ends when it does)
	write.reset();
	// stopped: the process killed, which ends its output (and the reading below)
	std::stop_callback cancel(stop, [hProcess = process.get()] { ::TerminateProcess(hProcess, ERROR_CANCELLED); });

	char buffer[4096];
	DWORD bytes;
	while (::ReadFile(read.get(), buffer, sizeof(buffer), &bytes, nullptr) && bytes)
		result.Output.append(buffer, bytes);
	::WaitForSingleObject(process.get(), INFINITE);
	::GetExitCodeProcess(process.get(), &result.ExitCode);
	result.Ran = true;
	result.Cancelled = stop.stop_requested();
	if (!result.Cancelled)
		result.Messages = Parse(result.Output);
	return result;
}

std::vector<InfVerifMessage> InfVerif::Parse(std::string_view output) {
	std::vector<InfVerifMessage> messages;
	while (!output.empty()) {
		auto end = output.find('\n');
		auto line = TrimEnd(output.substr(0, end));
		output.remove_prefix(end == std::string_view::npos ? output.size() : end + 1);

		// SEVERITY(code) in path, line N: text
		auto paren = line.find('(');
		auto close = line.find(')', paren);
		if (paren == std::string_view::npos || close == std::string_view::npos)
			continue;
		auto severity = line.substr(0, paren);
		InfVerifMessage message{};
		if (severity == "ERROR")
			message.Kind = InfVerifMessage::Severity::Error;
		else if (severity == "WARNING")
			message.Kind = InfVerifMessage::Severity::Warning;
		else if (severity == "INFO")
			message.Kind = InfVerifMessage::Severity::Info;
		else
			continue;
		if (!ParseInt(line.substr(paren + 1, close - paren - 1), message.Code))
			continue;

		// (the path may have commas; ", line " comes after it)
		auto where = line.find(", line ", close);
		if (where == std::string_view::npos)
			continue;
		auto colon = line.find(':', where);
		if (colon == std::string_view::npos || !ParseInt(line.substr(where + 7, colon - where - 7), message.Line))
			continue;
		auto text = line.substr(colon + 1);
		if (!text.empty() && text.front() == ' ')
			text.remove_prefix(1);
		// (a console program writes to a pipe in the OEM code page)
		message.Text.resize(text.size());
		message.Text.resize(::MultiByteToWideChar(CP_OEMCP, 0, text.data(), (int)text.size(), message.Text.data(), (int)message.Text.size()));
		messages.push_back(std::move(message));
	}
	return messages;
}
