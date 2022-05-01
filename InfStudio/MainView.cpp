#include "pch.h"
#include "MainView.h"
#include <fstream>

bool CMainView::OpenFile(PCWSTR path) {
	auto hFile = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	auto size = ::GetFileSize(hFile, nullptr);
	auto buffer = std::make_unique<char[]>(size + sizeof(WCHAR));
	DWORD bytes = 0;
	WORD bom;
	bool ok = false;
	if (::ReadFile(hFile, &bom, sizeof(bom), &bytes, nullptr)) {
		if (bom == 0xfeff)	// Unicode marker
			size -= sizeof(bom);
		else
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
		bytes = 0;
		if (::ReadFile(hFile, buffer.get(), size, &bytes, nullptr) && bytes) {
			if (bom == 0xfeff) {
				auto s = (WCHAR*)buffer.get();
				s[bytes / sizeof(WCHAR)] = 0;
				m_InfView.SetText(CStringA(s));
			}
			else {
				buffer[bytes] = 0;
				m_InfView.SetText(buffer.get());
			}
			m_Inf.Open(path, nullptr, &m_ErrorLine);
			ok = true;
		}
	}
	::CloseHandle(hFile);
	return ok;
}

LRESULT CMainView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_Tree.Create(m_Splitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
	m_InfView.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	m_Splitter.SetSplitterPanes(m_Tree, m_InfView);
	m_Splitter.SetSplitterPosPct(20);

	return 0;
}
