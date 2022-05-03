#include "pch.h"
#include "MainView.h"
#include "resource.h"

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
			m_InfView.SetSavePoint();
			m_Path = path;
			m_Name = m_Path.Mid(m_Path.ReverseFind(L'\\') + 1);
			BuildTree();
			ok = true;
		}
	}
	::CloseHandle(hFile);
	return ok;
}

bool CMainView::OnTreeDoubleClick(HWND, HTREEITEM hItem) {
	auto data = m_Tree.GetItemData(hItem);
	if (data >= (int)NodeType::Section) {
		//
		// find section
		//
		CString text;
		m_Tree.GetItemText(hItem, text);
		auto pos = m_InfView.FindText(Scintilla::FindOption::None, (PCSTR)CStringA(L"[" + text + L"]"));
		if (pos >= 0) {
			m_InfView.GotoPos(pos);
			m_InfView.SetSelectionStart(pos);
			m_InfView.SetSelectionEnd(pos + text.GetLength() + 2);
			PostMessage(WM_SETFOCUS);
			return true;
		}
	}
	return false;
}

void CMainView::OnActivated(bool active) {
	if (active)
		UpdateUI();
}

void CMainView::UpdateUI() {
	auto& ui = UI();
	ui.UIEnable(ID_EDIT_COPY, !m_InfView.IsSelectionEmpty());
	ui.UIEnable(ID_EDIT_PASTE, m_InfView.CanPaste());
	ui.UIEnable(ID_EDIT_CUT, !m_InfView.IsReadOnly() && !m_InfView.IsSelectionEmpty());
	ui.UIEnable(ID_EDIT_PASTE, m_InfView.CanPaste());
	ui.UIEnable(ID_EDIT_UNDO, m_InfView.CanUndo());
	ui.UIEnable(ID_EDIT_REDO, m_InfView.CanRedo());
	ui.UIEnable(ID_FILE_SAVE, m_InfView.IsModified());

	Frame()->SetStatusText(4, std::format(L"Line: {} Col: {}",
		m_InfView.LineNumber(m_InfView.CurrentPos()) + 1, m_InfView.Column(m_InfView.CurrentPos()) + 1).c_str());
}

bool CMainView::DoSave(PCWSTR path) {
	return false;
}

void CMainView::BuildTree() {
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	auto hRoot = InsertTreeItem(m_Tree, m_Name, TreeIconIndex::InfFile, NodeType::Root);
	if (!m_Path.IsEmpty() && !m_InfView.IsModified()) {
		//
		// analyze file
		//
		if (!m_Inf.Open(m_Path, nullptr, &m_ErrorLine)) {
			InsertTreeItem(m_Tree, std::format(L"Error in line {}", m_ErrorLine).c_str(), TreeIconIndex::Error, NodeType::Error, hRoot);
		}
		else {
			AnalyzeAndBuild(hRoot);
		}
	}
	m_Tree.Expand(hRoot, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CMainView::AnalyzeAndBuild(HTREEITEM hRoot) {
	if (m_Inf.SectionExists(L"version")) {
		InsertTreeItem(m_Tree, L"Version", TreeIconIndex::Version, NodeType::Version, hRoot);
	}
	auto mfg = m_Inf.GetSectionCompactLines(L"Manufacturer");
	if (!mfg.empty()) {
		auto hMfg = InsertTreeItem(m_Tree, L"Models", TreeIconIndex::Models, NodeType::Models, hRoot);
		for (auto& [key, value] : mfg) {
			for (auto& model : InfFile::GetStringPairs(value)) {
				auto hModel = InsertTreeItem(m_Tree, model.c_str(), TreeIconIndex::Model, NodeType::Model, hMfg);
				for (auto& [key, value] : m_Inf.GetSectionCompactLines(model.c_str())) {
					auto comma = value.find(L',');
					if (comma != std::wstring::npos) {
						auto section = value.substr(0, comma);
						static struct {
							PCWSTR Name;
							TreeIconIndex Icon;
							NodeType Type;
						} directives[] = {
							{ L"CopyFiles", TreeIconIndex::CopyFiles, NodeType::CopyFiles },
							{ L"DelFiles", TreeIconIndex::DelFiles, NodeType::DelFiles },
							{ L"AddReg", TreeIconIndex::AddReg, NodeType::AddReg },
							{ L"DelReg", TreeIconIndex::DelReg, NodeType::DelReg },
						};
						for (auto& directive : directives) {
							AddKnownDirective(hModel, section.c_str(), directive.Name, directive.Icon, directive.Type);
							AddKnownDirective(hModel, (section + L".NT").c_str(), directive.Name, directive.Icon, directive.Type);
							AddKnownDirective(hModel, (section + L".Hw").c_str(), directive.Name, directive.Icon, directive.Type);
							AddKnownDirective(hModel, (section + L"Ntamd64").c_str(), directive.Name, directive.Icon, directive.Type);
							AddKnownDirective(hModel, (section + L".Hw.NT").c_str(), directive.Name, directive.Icon, directive.Type);
						}
					}
				}
			}
		}
	}

	//
	// add all sections
	//
	auto hSections = InsertTreeItem(m_Tree, L"All Sections", TreeIconIndex::Sections, NodeType::Sections, hRoot);
	for (auto& sec : m_Inf.GetSectionNames()) {
		InsertTreeItem(m_Tree, sec.c_str(), TreeIconIndex::Section, NodeType::Section, hSections, TVI_SORT);
	}
}

LRESULT CMainView::OnSave(WORD, WORD, HWND, BOOL&) {
	if (m_Path.IsEmpty())
		return SendMessage(WM_COMMAND, ID_FILE_SAVE_AS);

	DoSave(m_Path);
	return 0;
}

LRESULT CMainView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_Tree.Create(m_Splitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
	m_InfView.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	static CImageList images;
	if (!images) {
		images.Create(m_TreeIconSize, m_TreeIconSize, ILC_COLOR32 | ILC_MASK, 16, 8);
		images.AddIcon(Frame()->GetInfIcon());
		images.AddIcon(AtlLoadSysIcon(IDI_ERROR));

		UINT icons[] = {
			IDI_SECTIONS, IDI_SECTION, IDI_REG_EDIT, IDI_REG_DELETE, IDI_DISK_EDIT, IDI_DISK_DELETE, IDI_VERSION,
			IDI_DEVICES, IDI_DEVICE,
		};
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, m_TreeIconSize, m_TreeIconSize));
	}
	m_Tree.SetImageList(images, TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_InfView);
	m_Splitter.SetSplitterPosPct(20);

	return 0;
}

int CMainView::AddKnownDirective(HTREEITEM hParent, PCWSTR section, PCWSTR directive, TreeIconIndex icon, NodeType type) {
	int count = 0;
	HTREEITEM hSection = FindChild(m_Tree, hParent, section);

	for (auto const& [key, value] : m_Inf.GetSectionCompactLines(section)) {
		if (_wcsicmp(key.c_str(), directive) != 0)
			continue;
		size_t pos = 0;
		while (true) {
			std::wstring name;
			auto comma = value.find(L',', pos);
			if (comma == std::wstring::npos) {
				name = value.substr(pos);
			}
			else {
				name = value.substr(pos, comma - pos);
				pos = comma + 1;
			}
			ATLASSERT(!name.empty());
			if (hSection == nullptr) {
				hSection = InsertTreeItem(m_Tree, section, TreeIconIndex::Section, NodeType::Section, hParent, TVI_SORT);
			}
			if (!FindChild(m_Tree, hSection, name.c_str())) {
				InsertTreeItem(m_Tree, name.c_str(), icon, type, hSection, TVI_SORT);
				count++;
			}
			if (comma == std::wstring::npos)
				break;
		}
	}
	return count;
}


LRESULT CMainView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_InfView.SetFocus();
	return 0;
}

LRESULT CMainView::OnTextChanged(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CMainView::OnModified(int, LPNMHDR, BOOL&) {
	UpdateUI();
	return 0;
}

LRESULT CMainView::OnTreeKeyDown(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTVKEYDOWN*)hdr;
	if (tv->wVKey == VK_TAB) {
		m_InfView.SetFocus();
		return 1;
	}

	return 0;
}
