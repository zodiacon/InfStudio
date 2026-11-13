#include "pch.h"
#include "MainView.h"
#include "resource.h"
#include <WTLHelper.h>
#include "TextFile.h"

BOOL CMainView::PreTranslateMessage(MSG* pMsg) {
	if (m_fr && m_fr->IsDialogMessage(pMsg))
		return TRUE;

	return FALSE;
}

bool CMainView::OpenFile(PCWSTR path) {
	std::string text;
	if (!TextFile::Load(path, text, m_Encoding))
		return false;

	m_InfView.SetText(text.c_str());
	m_InfView.EmptyUndoBuffer();
	m_InfView.SetSavePoint();
	m_InfView.UpdateLineNumberWidth();
	m_Path = path;
	m_Name = m_Path.Mid(m_Path.ReverseFind(L'\\') + 1);
	BuildTree();
	return true;
}

void CMainView::NewFromText(std::string const& text, PCWSTR name) {
	m_InfView.SetText(text.c_str());
	m_InfView.EmptyUndoBuffer();
	// (which leaves the editor unmodified: the text isn't in a file yet, so it counts as unsaved until it is)
	m_Unsaved = true;
	m_InfView.UpdateLineNumberWidth();
	m_Path.Empty();
	m_Name = name;
	m_Encoding = TextEncoding::Ansi;
	BuildTree();
}

bool CMainView::OnTreeDoubleClick(HWND, HTREEITEM hItem) {
	if (auto data = m_Tree.GetItemData(hItem); data >= (int)NodeType::Section) {
		//
		// find section
		//
		CString text;
		m_Tree.GetItemText(hItem, text);
		auto section = TextFile::ToUtf8(std::wstring(L"[" + text + L"]"));
		auto pos = m_InfView.FindText(Scintilla::FindOption::None, section.c_str());
		if (pos >= 0) {
			m_InfView.GotoPos(pos);
			m_InfView.SetSelectionStart(pos);
			m_InfView.SetSelectionEnd(pos + section.length());
			PostMessage(WM_SETFOCUS);
			return true;
		}
	}
	return false;
}

void CMainView::OnActivated(bool active) {
	if (active) {
		UpdateUI();
		UI().UIEnable(ID_VIEW_REFRESH, true);
		for (UINT id : { ID_EDIT_FIND, ID_EDIT_REPLACE, ID_EDIT_REPEAT, ID_EDIT_FIND_PREVIOUS, ID_INF_VALIDATE, ID_INF_VALIDATE_WHQL, ID_VIEW_RESULTS })
			UI().UIEnable(id, true);
		// (each tab has its own results pane)
		UI().UISetCheck(ID_VIEW_RESULTS, m_EditSplitter.GetSinglePaneMode() == SPLIT_PANE_NONE);
	}
}

bool CMainView::CanClose() {
	if (IsDirty()) {
		int result = AtlMessageBox(m_hWnd, L"Save file before close?", IDS_TITLE, MB_YESNOCANCEL | MB_DEFBUTTON1 | MB_ICONQUESTION);
		if (result == IDCANCEL)
			return false;
		if (result == IDYES) {
			SendMessage(WM_COMMAND, ID_FILE_SAVE);
			// not saved (failed, or cancelled in Save As): the file stays open
			if (IsDirty())
				return false;
		}
	}
	return true;
}

void CMainView::UpdateUI() {
	auto& ui = UI();
	ui.UIEnable(ID_EDIT_COPY, !m_InfView.IsSelectionEmpty());
	ui.UIEnable(ID_EDIT_PASTE, m_InfView.CanPaste());
	ui.UIEnable(ID_EDIT_CUT, !m_InfView.IsReadOnly() && !m_InfView.IsSelectionEmpty());
	ui.UIEnable(ID_EDIT_DELETE, !m_InfView.IsReadOnly() && !m_InfView.IsSelectionEmpty());
	ui.UIEnable(ID_EDIT_PASTE, m_InfView.CanPaste());
	ui.UIEnable(ID_EDIT_UNDO, m_InfView.CanUndo());
	ui.UIEnable(ID_EDIT_REDO, m_InfView.CanRedo());
	ui.UIEnable(ID_FILE_SAVE, IsDirty());

	Frame()->SetStatusText(4, std::format(L"Line: {} Col: {}",
		m_InfView.LineNumber(m_InfView.CurrentPos()) + 1, m_InfView.Column(m_InfView.CurrentPos()) + 1).c_str());
}

bool CMainView::ConfirmEncoding() {
	if (m_Encoding != TextEncoding::Ansi || TextFile::FitsAnsi(m_InfView.GetText(m_InfView.Length())))
		return true;

	// as ANSI, the characters the code page doesn't have would be saved as '?'
	auto result = AtlMessageBox(m_hWnd,
		L"The file has characters that can't be saved in the ANSI code page.\n\nSave it as Unicode (UTF-16) instead?",
		IDS_TITLE, MB_YESNOCANCEL | MB_ICONWARNING);
	if (result == IDCANCEL)
		return false;
	if (result == IDYES)
		m_Encoding = TextEncoding::Utf16;
	return true;
}

bool CMainView::DoSave(PCWSTR path) {
	if (!TextFile::Save(path, m_InfView.GetText(m_InfView.Length()), m_Encoding))
		return false;

	m_InfView.SetSavePoint();
	m_Unsaved = false;
	return true;
}

void CMainView::BuildTree() {
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	auto hRoot = InsertTreeItem(m_Tree, m_Name, TreeIconIndex::InfFile, NodeType::Root);
	if (!m_Path.IsEmpty() && !IsDirty()) {
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
		auto hMfg = InsertTreeItem(m_Tree, L"Manufacturer", TreeIconIndex::Models, NodeType::Models, hRoot);
		for (auto& [key, value] : mfg) {
			for (auto const& model : InfFile::GetModelSections(value)) {
				auto hModel = InsertTreeItem(m_Tree, model.c_str(), TreeIconIndex::Model, NodeType::Model, hMfg);
				// each device's install section (the first field of its value), in all its decorations
				for (auto const& [key, value] : m_Inf.GetSectionCompactLines(model.c_str())) {
					auto install = value.substr(0, value.find(L','));
					for (auto const& d : m_Inf.GetInstallDirectives(install)) {
						static const struct {
							PCWSTR Name;
							TreeIconIndex Icon;
							NodeType Type;
						} kinds[] = {
							{ L"CopyFiles", TreeIconIndex::CopyFiles, NodeType::CopyFiles },
							{ L"DelFiles", TreeIconIndex::DelFiles, NodeType::DelFiles },
							{ L"AddReg", TreeIconIndex::AddReg, NodeType::AddReg },
							{ L"DelReg", TreeIconIndex::DelReg, NodeType::DelReg },
						};
						auto kind = std::find_if(std::begin(kinds), std::end(kinds), [&](auto const& k) { return d.Directive == k.Name; });
						ATLASSERT(kind != std::end(kinds));
						auto hSection = FindChild(m_Tree, hModel, d.Section.c_str());
						if (!hSection)
							hSection = InsertTreeItem(m_Tree, d.Section.c_str(), TreeIconIndex::Section, NodeType::Section, hModel, TVI_SORT);
						if (!FindChild(m_Tree, hSection, d.Target.c_str()))
							InsertTreeItem(m_Tree, d.Target.c_str(), kind->Icon, kind->Type, hSection, TVI_SORT);
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

	if (!IsDirty())
		return false;

	if (ConfirmEncoding() && !DoSave(m_Path))
		DisplaySaveError();
	return 0;
}

LRESULT CMainView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	m_Tree.Create(m_Splitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);
	m_EditSplitter.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_InfView.Create(m_EditSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_Results.Create(m_EditSplitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, IDC_RESULTS);
	m_Results.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Results.InsertColumn(0, L"Line", LVCFMT_RIGHT, 70);
	m_Results.InsertColumn(1, L"Code", LVCFMT_LEFT, 60);
	m_Results.InsertColumn(2, L"Problem", LVCFMT_LEFT, 900);
	{
		// error, warning, information (InfVerifMessage::Severity's order)
		CImageList icons;
		icons.Create(16, 16, ILC_COLOR32 | ILC_MASK, 3, 0);
		for (auto id : { IDI_ERROR, IDI_WARNING, IDI_INFORMATION }) {
			HICON hIcon = nullptr;
			::LoadIconWithScaleDown(nullptr, id, 16, 16, &hIcon);
			icons.AddIcon(hIcon);
			::DestroyIcon(hIcon);
		}
		m_Results.SetImageList(icons, LVSIL_SMALL);
	}
	m_EditSplitter.SetSplitterPanes(m_InfView, m_Results);
	m_EditSplitter.SetSplitterPosPct(75);
	// (shown once there are results)
	m_EditSplitter.SetSinglePaneMode(SPLIT_PANE_TOP);

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

	m_Splitter.SetSplitterPanes(m_Tree, m_EditSplitter);
	m_Splitter.SetSplitterPosPct(20);

	return 0;
}

void CMainView::DisplaySaveError() {
	AtlMessageBox(m_hWnd, L"Failed to save file", IDS_TITLE, MB_ICONERROR);
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

LRESULT CMainView::OnEditFind(WORD, WORD, HWND, BOOL&) {
	ShowFindDialog(false);
	return 0;
}

LRESULT CMainView::OnEditReplace(WORD, WORD, HWND, BOOL&) {
	ShowFindDialog(true);
	return 0;
}

CString CMainView::SelectedText() {
	auto start = m_InfView.Execute(SCI_GETSELECTIONSTART);
	auto end = m_InfView.Execute(SCI_GETSELECTIONEND);
	if (start == end || end - start > 256)
		return L"";
	auto text = m_InfView.GetText(m_InfView.Length()).substr(start, end - start);
	if (text.find_first_of("\r\n") != std::string::npos)
		return L"";
	return TextFile::FromUtf8(text).c_str();
}

void CMainView::ShowFindDialog(bool replace) {
	if (auto selected = SelectedText(); !selected.IsEmpty())
		m_SearchText = selected;

	// Find and Replace are different dialogs: the other one goes
	if (m_fr && m_frReplace != replace) {
		m_fr->DestroyWindow();
		m_fr = nullptr;
	}
	if (!m_fr) {
		DWORD flags = (m_Search.Down ? FR_DOWN : 0) | (m_Search.MatchCase ? FR_MATCHCASE : 0) | (m_Search.WholeWord ? FR_WHOLEWORD : 0);
		m_fr = new CFindReplaceDialog;
		m_fr->Create(!replace, m_SearchText, m_ReplaceText, flags, m_hWnd);
		m_frReplace = replace;
	}
	else {
		m_fr->SetDlgItemText(edt1, m_SearchText);
	}
	m_fr->ShowWindow(SW_SHOW);
	m_fr->SetFocus();
}

LRESULT CMainView::OnFind(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (m_fr->IsTerminating()) {
		m_fr = nullptr;
		return 0;
	}

	m_SearchText = m_fr->GetFindString();
	m_ReplaceText = m_fr->GetReplaceString();
	m_Search = { m_fr->MatchCase() != FALSE, m_fr->MatchWholeWord() != FALSE, m_fr->SearchDown() != FALSE };
	auto text = TextFile::ToUtf8((PCWSTR)m_SearchText);
	auto replacement = TextFile::ToUtf8((PCWSTR)m_ReplaceText);

	bool found;
	if (m_fr->ReplaceAll()) {
		auto count = EditorSearch::ReplaceAll(m_InfView, text, replacement, m_Search);
		Frame()->SetStatusText(0, std::format(L"{} replaced", count).c_str());
		found = count > 0;
	}
	else if (m_fr->ReplaceCurrent()) {
		found = EditorSearch::Replace(m_InfView, text, replacement, m_Search);
	}
	else {
		found = EditorSearch::FindNext(m_InfView, text, m_Search);
	}
	if (!found) {
		Frame()->SetStatusText(0, L"Not found");
		::MessageBeep(MB_OK);
	}
	return 0;
}

LRESULT CMainView::OnFindNext(WORD, WORD wID, HWND, BOOL&) {
	if (m_SearchText.IsEmpty()) {
		ShowFindDialog(false);
		return 0;
	}
	auto options = m_Search;
	options.Down = wID == ID_EDIT_REPEAT;
	if (!EditorSearch::FindNext(m_InfView, TextFile::ToUtf8((PCWSTR)m_SearchText), options)) {
		Frame()->SetStatusText(0, L"Not found");
		::MessageBeep(MB_OK);
	}
	return 0;
}

LRESULT CMainView::OnSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(FALSE, L"inf", m_Name, OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT,
		L"INF Files\0*.inf;*.inx\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto result = dlg.DoModal(m_hWnd);
	WTLHelper::ResumeHook();
	if (result == IDOK && ConfirmEncoding()) {
		if (DoSave(dlg.m_szFileName)) {
			// the view's file is the new one only once it's there
			m_Path = dlg.m_szFileName;
			m_Name = dlg.m_szFileTitle;
			Frame()->UpdateTabTitle(m_hWnd, m_Name);
			// (a new file's tree is analyzed now that it is in a file)
			BuildTree();
		}
		else {
			DisplaySaveError();
		}
	}
	return 0;
}

LRESULT CMainView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	SendMessage(WM_COMMAND, ID_FILE_SAVE);
	CWaitCursor wait;
	BuildTree();
	return 0;
}

LRESULT CMainView::OnValidate(WORD, WORD wID, HWND, BOOL&) {
	Validate(wID == ID_INF_VALIDATE_WHQL ? L"/h" : L"/w");
	return 0;
}

void CMainView::Validate(PCWSTR mode) {
	auto infverif = InfVerif::Find();
	if (infverif.empty()) {
		AtlMessageBox(m_hWnd, L"InfVerif.exe, which validates INF files, wasn't found.\n\nIt comes with the Windows Driver Kit (WDK).",
			IDS_TITLE, MB_ICONINFORMATION);
		return;
	}

	// The editor's text, saved or not, goes to a file of its own by the same name (InfVerif checks the name too).
	// An ANSI file with characters the code page lacks goes as UTF-16, so that they get there.
	auto text = m_InfView.GetText(m_InfView.Length());
	auto encoding = m_Encoding == TextEncoding::Ansi && !TextFile::FitsAnsi(text) ? TextEncoding::Utf16 : m_Encoding;
	std::error_code ec;
	auto dir = std::filesystem::temp_directory_path(ec) / std::format(L"InfStudio-{}", ::GetCurrentProcessId());
	std::filesystem::create_directories(dir, ec);
	auto file = dir / (m_Name.IsEmpty() ? L"Untitled.inf" : (PCWSTR)m_Name);
	if (!TextFile::Save(file.c_str(), text, encoding)) {
		AtlMessageBox(m_hWnd, L"Failed to write the file for InfVerif.", IDS_TITLE, MB_ICONERROR);
		return;
	}

	InfVerifResult result;
	{
		CWaitCursor wait;
		Frame()->SetStatusText(0, L"Running InfVerif...");
		result = InfVerif::Run(infverif, mode, file);
	}
	std::filesystem::remove_all(dir, ec);
	if (!result.Ran) {
		Frame()->SetStatusText(0, L"");
		AtlMessageBox(m_hWnd, std::format(L"Failed to run {}.", infverif.wstring()).c_str(), IDS_TITLE, MB_ICONERROR);
		return;
	}
	ShowResults(std::move(result.Messages), mode);
}

void CMainView::ShowResults(std::vector<InfVerifMessage> messages, PCWSTR mode) {
	m_Messages = std::move(messages);
	m_Results.SetRedraw(FALSE);
	m_Results.DeleteAllItems();
	int errors = 0, warnings = 0;
	std::vector<CInfView::Problem> problems;
	for (int i = 0; i < (int)m_Messages.size(); i++) {
		auto const& m = m_Messages[i];
		auto n = m_Results.InsertItem(i, m.Line > 0 ? std::to_wstring(m.Line).c_str() : L"", (int)m.Kind);
		m_Results.SetItemText(n, 1, std::to_wstring(m.Code).c_str());
		m_Results.SetItemText(n, 2, m.Text.c_str());
		m_Results.SetItemData(n, i);
		if (m.Kind == InfVerifMessage::Severity::Error)
			errors++;
		else if (m.Kind == InfVerifMessage::Severity::Warning)
			warnings++;
		if (m.Kind != InfVerifMessage::Severity::Info)
			problems.push_back({ m.Line, m.Kind == InfVerifMessage::Severity::Error });
	}
	if (m_Messages.empty()) {
		auto n = m_Results.InsertItem(0, L"", (int)InfVerifMessage::Severity::Info);
		m_Results.SetItemText(n, 2, L"No problems found");
		m_Results.SetItemData(n, (DWORD_PTR)-1);
	}
	m_Results.SetRedraw(TRUE);
	m_InfView.ShowProblems(problems);
	ShowResultsPane(true);
	Frame()->SetStatusText(0, std::format(L"InfVerif {}: {} error{}, {} warning{}", mode,
		errors, errors == 1 ? L"" : L"s", warnings, warnings == 1 ? L"" : L"s").c_str());
}

void CMainView::ShowResultsPane(bool show) {
	m_EditSplitter.SetSinglePaneMode(show ? SPLIT_PANE_NONE : SPLIT_PANE_TOP);
	UI().UISetCheck(ID_VIEW_RESULTS, show);
}

LRESULT CMainView::OnViewResults(WORD, WORD, HWND, BOOL&) {
	ShowResultsPane(m_EditSplitter.GetSinglePaneMode() != SPLIT_PANE_NONE);
	return 0;
}

LRESULT CMainView::OnResultActivate(int, LPNMHDR, BOOL&) {
	auto selected = m_Results.GetSelectedIndex();
	if (selected < 0)
		return 0;
	auto index = (int)m_Results.GetItemData(selected);
	if (index >= 0 && index < (int)m_Messages.size())
		GoToLine(m_Messages[index].Line);
	return 0;
}

void CMainView::GoToLine(int line) {
	if (line < 1)
		return;
	m_InfView.Execute(SCI_ENSUREVISIBLE, line - 1);
	m_InfView.Execute(SCI_GOTOLINE, line - 1);
	m_InfView.Execute(SCI_VERTICALCENTRECARET);
	m_InfView.SetFocus();
}
