#include "pch.h"
#include "MainView.h"
#include "resource.h"
#include <WTLHelper.h>
#include "TextFile.h"
#include <IconHelper.h>
#include <ListViewhelper.h>

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
	SetReadOnly(TextFile::ShouldOpenReadOnly(path));
	m_Path = path;
	m_Name = m_Path.Mid(m_Path.ReverseFind(L'\\') + 1);
	BuildTree();
	return true;
}

void CMainView::FontChanged() {
	m_InfView.FontChanged();
	m_InfView.UpdateLineNumberWidth();
}

void CMainView::ApplyDpi() {
	auto dpi = DpiHelper::GetDpi(m_hWnd);

	// the system's message font, at the DPI
	CFont font(DpiHelper::CreateMessageFont(dpi));
	m_Tree.SetFont(font);
	m_Results.SetFont(font);
	if (m_UiFont)
		m_UiFont.DeleteObject();
	m_UiFont.Attach(font.Detach());

	// the icons at the small icon size of the DPI, in TreeIconIndex's order
	auto size = DpiHelper::SmallIconSize(dpi);
	auto systemIcon = [&](PCWSTR id) {
		HICON hIcon = nullptr;
		::LoadIconWithScaleDown(nullptr, id, size, size, &hIcon);
		return hIcon;
	};
	CImageList tree;
	tree.Create(size, size, ILC_COLOR32 | ILC_MASK, 16, 8);
	tree.AddIcon(Frame()->GetInfIcon());
	tree.AddIcon(CIcon(systemIcon(IDI_ERROR)));
	for (UINT icon : { IDI_SECTIONS, IDI_SECTION, IDI_REG_EDIT, IDI_REG_DELETE, IDI_DISK_EDIT, IDI_DISK_DELETE, IDI_VERSION, IDI_DEVICES, IDI_DEVICE })
		tree.AddIcon(CIcon(IconHelper::Load((int)icon, size)));
	m_Tree.SetImageList(tree, TVSIL_NORMAL);
	if (m_TreeImages)
		m_TreeImages.Destroy();
	m_TreeImages.Attach(tree.Detach());

	// error, warning, information (InfVerifMessage::Severity's order)
	CImageList results;
	results.Create(size, size, ILC_COLOR32 | ILC_MASK, 3, 0);
	for (auto id : { IDI_ERROR, IDI_WARNING, IDI_INFORMATION })
		results.AddIcon(CIcon(systemIcon(id)));
	m_Results.SetImageList(results, LVSIL_SMALL);
	if (m_ResultImages)
		m_ResultImages.Destroy();
	m_ResultImages.Attach(results.Detach());

	m_InfView.UpdateMargins();
}

void CMainView::OnDpiChanged(UINT) {
	ApplyDpi();
}

void CMainView::LiveAnalysisChanged() {
	// on: the tree catches up with the text now; off: no update pending
	if (AppSettings::Get().LiveAnalysis())
		BuildTree();
	else
		KillTimer(AnalysisTimer);
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
		UI().UIEnable(ID_VIEW_REFRESH, true);
		for (UINT id : { ID_FILE_SAVE_AS, ID_EDIT_FIND, ID_EDIT_REPLACE, ID_EDIT_REPEAT, ID_EDIT_FIND_PREVIOUS, ID_INF_VALIDATE, ID_INF_VALIDATE_WHQL, ID_VIEW_RESULTS,
			ID_EDIT_GOTO_DEFINITION, ID_EDIT_GO_BACK, ID_EDIT_COMPLETE, ID_ENCODING_ANSI, ID_ENCODING_UTF8, ID_ENCODING_UTF16 })
			UI().UIEnable(id, true);
		// (each tab has its own results pane)
		UI().UISetCheck(ID_VIEW_RESULTS, m_EditSplitter.GetSinglePaneMode() == SPLIT_PANE_NONE);
		UI().UIEnable(ID_FILE_READONLY, true);
		// (after the above: a read-only file turns some of them off)
		UpdateUI();
		// (what another tab said doesn't apply)
		Frame()->SetStatusText(0, L"Ready");
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
	ui.UIEnable(ID_FILE_SAVE, IsDirty() && !m_ReadOnly);
	ui.UISetCheck(ID_FILE_READONLY, m_ReadOnly);
	for (UINT id : { ID_EDIT_REPLACE, ID_EDIT_COMPLETE, ID_ENCODING_ANSI, ID_ENCODING_UTF8, ID_ENCODING_UTF16 })
		ui.UIEnable(id, !m_ReadOnly);

	static constexpr PCWSTR encodings[] = { L"ANSI", L"UTF-8", L"UTF-16" };
	Frame()->SetStatusText(1, encodings[(int)m_Encoding]);
	Frame()->SetStatusText(2, m_ReadOnly ? L"Read-only" : IsDirty() ? L"Modified" : L"");
	Frame()->SetStatusText(3, m_ProblemsText.c_str());
	ui.UISetRadioMenuItem(ID_ENCODING_ANSI + (int)m_Encoding, ID_ENCODING_ANSI, ID_ENCODING_UTF16);
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

std::filesystem::path CMainView::WriteTempCopy() {
	// An ANSI file with characters the code page lacks goes as UTF-16, so that they get there.
	auto text = m_InfView.GetText(m_InfView.Length());
	auto encoding = m_Encoding == TextEncoding::Ansi && !TextFile::FitsAnsi(text) ? TextEncoding::Utf16 : m_Encoding;
	std::error_code ec;
	// (a folder for each view, as two may have files of the same name)
	auto dir = std::filesystem::temp_directory_path(ec) / std::format(L"InfStudio-{}", ::GetCurrentProcessId()) / std::format(L"{}", (void*)this);
	std::filesystem::create_directories(dir, ec);
	auto file = dir / (m_Name.IsEmpty() ? L"Untitled.inf" : (PCWSTR)m_Name);
	if (!TextFile::Save(file.c_str(), text, encoding))
		return {};
	return file;
}

void CMainView::DeleteTempCopy(std::filesystem::path const& file) {
	std::error_code ec;
	std::filesystem::remove_all(file.parent_path(), ec);
}

std::vector<std::wstring> CMainView::TreePath(HTREEITEM hItem) const {
	std::vector<std::wstring> path;
	for (; hItem; hItem = m_Tree.GetParentItem(hItem)) {
		CString text;
		m_Tree.GetItemText(hItem, text);
		path.insert(path.begin(), (PCWSTR)text);
	}
	return path;
}

void CMainView::SaveTreeState(HTREEITEM hItem, TreeState& state) const {
	for (; hItem; hItem = m_Tree.GetNextSiblingItem(hItem)) {
		if (m_Tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED)
			state.Expanded.insert(TreePath(hItem));
		SaveTreeState(m_Tree.GetChildItem(hItem), state);
	}
}

void CMainView::RestoreTreeState(HTREEITEM hItem, TreeState const& state) {
	for (; hItem; hItem = m_Tree.GetNextSiblingItem(hItem)) {
		auto path = TreePath(hItem);
		if (state.Expanded.contains(path))
			m_Tree.Expand(hItem, TVE_EXPAND);
		if (path == state.Selected)
			m_Tree.SelectItem(hItem);
		RestoreTreeState(m_Tree.GetChildItem(hItem), state);
	}
}

void CMainView::BuildTree() {
	// what was open and selected, to open and select again
	TreeState state;
	auto hOldRoot = m_Tree.GetRootItem();
	if (hOldRoot) {
		SaveTreeState(hOldRoot, state);
		state.Selected = TreePath(m_Tree.GetSelectedItem());
		// (the root keeps its place when the file's name changes)
		CString name;
		m_Tree.GetItemText(hOldRoot, name);
		if (name != m_Name) {
			auto rename = [&](std::vector<std::wstring>& path) {
				if (!path.empty())
					path[0] = (PCWSTR)m_Name;
			};
			std::set<std::vector<std::wstring>> expanded;
			for (auto path : state.Expanded) {
				rename(path);
				expanded.insert(std::move(path));
			}
			state.Expanded = std::move(expanded);
			rename(state.Selected);
		}
	}

	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	auto hRoot = InsertTreeItem(m_Tree, m_Name, TreeIconIndex::InfFile, NodeType::Root);

	// the editor's text, saved or not
	RunChecks(m_InfView.GetText(m_InfView.Length()));
	if (auto file = WriteTempCopy(); !file.empty()) {
		InfFile inf;
		UINT errorLine = 0;
		if (inf.Open(file.c_str(), nullptr, &errorLine))
			AddNodes(hRoot, AnalyzeInf(inf));
		else
			InsertTreeItem(m_Tree, std::format(L"Error in line {}", errorLine).c_str(), TreeIconIndex::Error, NodeType::Error, hRoot);
		inf.Close();
		DeleteTempCopy(file);
	}

	m_Tree.Expand(hRoot, TVE_EXPAND);
	RestoreTreeState(hRoot, state);
	m_Tree.SetRedraw(TRUE);
}

void CMainView::AddNodes(HTREEITEM hParent, std::vector<InfNode> const& nodes) {
	static const struct {
		InfNodeKind Kind;
		TreeIconIndex Icon;
		NodeType Type;
	} kinds[] = {
		{ InfNodeKind::Version, TreeIconIndex::Version, NodeType::Version },
		{ InfNodeKind::Manufacturer, TreeIconIndex::Models, NodeType::Models },
		{ InfNodeKind::Models, TreeIconIndex::Model, NodeType::Model },
		{ InfNodeKind::InstallSection, TreeIconIndex::Section, NodeType::Section },
		{ InfNodeKind::CopyFiles, TreeIconIndex::CopyFiles, NodeType::CopyFiles },
		{ InfNodeKind::DelFiles, TreeIconIndex::DelFiles, NodeType::DelFiles },
		{ InfNodeKind::AddReg, TreeIconIndex::AddReg, NodeType::AddReg },
		{ InfNodeKind::DelReg, TreeIconIndex::DelReg, NodeType::DelReg },
		{ InfNodeKind::AllSections, TreeIconIndex::Sections, NodeType::Sections },
		{ InfNodeKind::Section, TreeIconIndex::Section, NodeType::Section },
	};
	for (auto const& node : nodes) {
		auto kind = std::find_if(std::begin(kinds), std::end(kinds), [&](auto const& k) { return k.Kind == node.Kind; });
		ATLASSERT(kind != std::end(kinds));
		auto hItem = InsertTreeItem(m_Tree, node.Text.c_str(), kind->Icon, kind->Type, hParent);
		AddNodes(hItem, node.Children);
	}
}

LRESULT CMainView::OnSave(WORD, WORD, HWND, BOOL&) {
	if (m_Path.IsEmpty())
		return SendMessage(WM_COMMAND, ID_FILE_SAVE_AS);

	if (!IsDirty())
		return false;
	if (m_ReadOnly) {
		// (Ctrl+S comes even while Save is off)
		Frame()->SetStatusText(0, L"The file is read-only: Save As saves a copy");
		::MessageBeep(MB_OK);
		return 0;
	}

	if (!ConfirmEncoding())
		return 0;
	if (!DoSave(m_Path))
		DisplaySaveError();
	else if (!AppSettings::Get().LiveAnalysis())
		BuildTree();		// (without live analysis, the tree catches up on Save)
	UpdateUI();
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
	// (widths at 96 DPI, scaled to the window's; ListViewHelper scales them when the DPI changes)
	auto dpi = DpiHelper::GetDpi(m_hWnd);
	m_Results.InsertColumn(0, L"Line", LVCFMT_RIGHT, DpiHelper::Scale(70, dpi));
	m_Results.InsertColumn(1, L"Code", LVCFMT_LEFT, DpiHelper::Scale(60, dpi));
	m_Results.InsertColumn(2, L"Problem", LVCFMT_LEFT, DpiHelper::Scale(900, dpi));
	ListViewHelper::EnableColumnDpiScaling(m_Results);
	m_EditSplitter.SetSplitterPanes(m_InfView, m_Results);
	m_EditSplitter.SetSplitterPosPct(75);
	// (shown once there are results)
	m_EditSplitter.SetSinglePaneMode(SPLIT_PANE_TOP);

	ApplyDpi();
	AppliedDpi(dpi);

	m_InfView.SetCtrlClickHandler([this](LRESULT pos) { return GoToDefinition(pos); });
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

LRESULT CMainView::OnModified(int, LPNMHDR hdr, BOOL&) {
	if (reinterpret_cast<SCNotification*>(hdr)->updated & SC_UPDATE_SELECTION)
		FollowCaret();
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
			SetReadOnly(TextFile::ShouldOpenReadOnly(m_Path));
		}
		else {
			DisplaySaveError();
		}
	}
	return 0;
}

LRESULT CMainView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	// (the tree is of the editor's text, saved or not: nothing to save first)
	KillTimer(AnalysisTimer);
	CWaitCursor wait;
	BuildTree();
	return 0;
}

LRESULT CMainView::OnTextModified(int, LPNMHDR hdr, BOOL& bHandled) {
	auto scn = reinterpret_cast<SCNotification*>(hdr);
	if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
		// the tree again once typing pauses (Options > Live Analysis)
		if (AppSettings::Get().LiveAnalysis())
			SetTimer(AnalysisTimer, AnalysisDelay);
		if (scn->linesAdded)
			m_InfView.UpdateLineNumberWidth();
	}
	bHandled = FALSE;
	return 0;
}

LRESULT CMainView::OnTimer(UINT, WPARAM id, LPARAM, BOOL& bHandled) {
	if (id != AnalysisTimer) {
		bHandled = FALSE;
		return 0;
	}
	KillTimer(AnalysisTimer);
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
	auto file = WriteTempCopy();
	if (file.empty()) {
		AtlMessageBox(m_hWnd, L"Failed to write the file for InfVerif.", IDS_TITLE, MB_ICONERROR);
		return;
	}

	InfVerifResult result;
	{
		CWaitCursor wait;
		Frame()->SetStatusText(0, L"Running InfVerif...");
		result = InfVerif::Run(infverif, mode, file);
	}
	DeleteTempCopy(file);
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

bool CMainView::GoToDefinition(LRESULT pos) {
	auto definition = InfNavigation::FindDefinition(m_InfView.GetText(m_InfView.Length()), pos);
	if (definition == std::string::npos) {
		Frame()->SetStatusText(0, L"Nothing to go to");
		return false;
	}
	m_BackPositions.push_back(m_InfView.Execute(SCI_GETCURRENTPOS));
	if (m_BackPositions.size() > 100)
		m_BackPositions.erase(m_BackPositions.begin());
	m_InfView.Execute(SCI_ENSUREVISIBLE, m_InfView.Execute(SCI_LINEFROMPOSITION, definition));
	m_InfView.Execute(SCI_GOTOPOS, definition);
	m_InfView.Execute(SCI_VERTICALCENTRECARET);
	m_InfView.SetFocus();
	return true;
}

LRESULT CMainView::OnGoToDefinition(WORD, WORD, HWND, BOOL&) {
	if (!GoToDefinition(m_InfView.Execute(SCI_GETCURRENTPOS)))
		::MessageBeep(MB_OK);
	return 0;
}

LRESULT CMainView::OnGoBack(WORD, WORD, HWND, BOOL&) {
	if (m_BackPositions.empty()) {
		::MessageBeep(MB_OK);
		return 0;
	}
	// (the text may have changed since: within it)
	auto pos = std::min(m_BackPositions.back(), m_InfView.Execute(SCI_GETLENGTH));
	m_BackPositions.pop_back();
	m_InfView.Execute(SCI_ENSUREVISIBLE, m_InfView.Execute(SCI_LINEFROMPOSITION, pos));
	m_InfView.Execute(SCI_GOTOPOS, pos);
	m_InfView.Execute(SCI_VERTICALCENTRECARET);
	m_InfView.SetFocus();
	return 0;
}

LRESULT CMainView::OnDwellStart(int, LPNMHDR hdr, BOOL&) {
	auto pos = reinterpret_cast<SCNotification*>(hdr)->position;
	if (pos < 0)
		return 0;
	// a problem the checks found there: what it is
	for (auto const& problem : m_CheckProblems)
		if ((size_t)pos >= problem.Start && (size_t)pos < problem.End) {
			m_InfView.Execute(SCI_CALLTIPSHOW, pos, reinterpret_cast<LPARAM>(problem.Message.c_str()));
			return 0;
		}
	if (auto value = InfNavigation::StringValueAt(m_InfView.GetText(m_InfView.Length()), pos))
		m_InfView.Execute(SCI_CALLTIPSHOW, pos, reinterpret_cast<LPARAM>(value->c_str()));
	return 0;
}

LRESULT CMainView::OnDwellEnd(int, LPNMHDR, BOOL&) {
	m_InfView.Execute(SCI_CALLTIPCANCEL);
	return 0;
}

std::wstring CMainView::SectionAtCaret() {
	// the closest section header above the caret (the lexer has styled everything up to what's shown)
	for (auto line = m_InfView.Execute(SCI_LINEFROMPOSITION, m_InfView.Execute(SCI_GETCURRENTPOS)); line >= 0; line--) {
		auto start = m_InfView.Execute(SCI_GETLINEINDENTPOSITION, line);
		if (m_InfView.Execute(SCI_GETCHARAT, start) != '[' || m_InfView.Execute(SCI_GETSTYLEAT, start) != InfLexer::Section)
			continue;
		auto end = m_InfView.Execute(SCI_GETLINEENDPOSITION, line);
		std::string text(end - start + 1, '\0');
		Sci_TextRangeFull range{ { start, end }, text.data() };
		m_InfView.Execute(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&range));
		text.resize(end - start);
		auto close = text.find(']');
		auto name = text.substr(1, close == std::string::npos ? std::string::npos : close - 1);
		auto first = name.find_first_not_of(" \t");
		if (first == std::string::npos)
			return {};
		name = name.substr(first, name.find_last_not_of(" \t") - first + 1);
		return TextFile::FromUtf8(name);
	}
	return {};
}

void CMainView::FollowCaret() {
	auto section = SectionAtCaret();
	if (section == m_CaretSection)
		return;
	m_CaretSection = section;
	if (section.empty())
		return;

	if (auto hSelected = m_Tree.GetSelectedItem()) {
		CString selected;
		m_Tree.GetItemText(hSelected, selected);
		if (_wcsicmp(selected, section.c_str()) == 0)
			return;
	}
	// the section under All Sections
	for (auto hItem = m_Tree.GetChildItem(m_Tree.GetRootItem()); hItem; hItem = m_Tree.GetNextSiblingItem(hItem)) {
		if (m_Tree.GetItemData(hItem) != (DWORD_PTR)NodeType::Sections)
			continue;
		for (auto hSection = m_Tree.GetChildItem(hItem); hSection; hSection = m_Tree.GetNextSiblingItem(hSection)) {
			CString name;
			m_Tree.GetItemText(hSection, name);
			if (_wcsicmp(name, section.c_str()) == 0) {
				m_Tree.SelectItem(hSection);
				m_Tree.EnsureVisible(hSection);
				return;
			}
		}
	}
}

void CMainView::ShowCompletion(bool asked) {
	auto caret = (size_t)m_InfView.Execute(SCI_GETCURRENTPOS);
	auto list = InfCompletion::Complete(m_InfView.GetText(m_InfView.Length()), caret);
	if (!list || list->Items.empty()) {
		if (asked)
			::MessageBeep(MB_OK);
		return;
	}
	std::string items;
	for (auto const& item : list->Items) {
		if (!items.empty())
			items += '\n';
		items += item;
	}
	m_CompletionSuffix = list->Suffix;
	m_InfView.Execute(SCI_AUTOCSHOW, caret - list->Start, reinterpret_cast<LPARAM>(items.c_str()));
}

LRESULT CMainView::OnCharAdded(int, LPNMHDR hdr, BOOL&) {
	if (m_InfView.Execute(SCI_AUTOCACTIVE))
		return 0;
	auto ch = reinterpret_cast<SCNotification*>(hdr)->ch;
	auto isWordChar = [](int c) { return isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-' || c == '$'; };
	if (ch == '%' || ch == '[') {
		ShowCompletion(false);
		return 0;
	}
	// the first letter of a word
	auto caret = m_InfView.Execute(SCI_GETCURRENTPOS);
	auto previous = caret >= 2 ? (int)m_InfView.Execute(SCI_GETCHARAT, caret - 2) : 0;
	if (ch > 0 && ch < 128 && isWordChar(ch) && !isWordChar(previous))
		ShowCompletion(false);
	return 0;
}

LRESULT CMainView::OnAutoCompleted(int, LPNMHDR, BOOL&) {
	auto suffix = std::move(m_CompletionSuffix);
	m_CompletionSuffix.clear();
	if (suffix.empty())
		return 0;
	auto caret = m_InfView.Execute(SCI_GETCURRENTPOS);
	m_InfView.Execute(SCI_INSERTTEXT, caret, reinterpret_cast<LPARAM>(suffix.c_str()));
	m_InfView.Execute(SCI_GOTOPOS, caret + suffix.size());
	// a key: its value next, if it names sections
	if (suffix == " = ")
		ShowCompletion(false);
	return 0;
}

LRESULT CMainView::OnComplete(WORD, WORD, HWND, BOOL&) {
	ShowCompletion(true);
	return 0;
}

LRESULT CMainView::OnEncoding(WORD, WORD wID, HWND, BOOL&) {
	auto encoding = static_cast<TextEncoding>(wID - ID_ENCODING_ANSI);
	if (encoding == m_Encoding)
		return 0;
	m_Encoding = encoding;
	m_Unsaved = true;
	UpdateUI();
	return 0;
}

void CMainView::RunChecks(std::string const& text) {
	m_CheckProblems = InfChecks::Check(text);
	std::vector<CInfView::Range> ranges;
	int errors = 0, warnings = 0;
	for (auto const& p : m_CheckProblems) {
		ranges.push_back({ (LRESULT)p.Start, (LRESULT)p.End, p.Error });
		(p.Error ? errors : warnings)++;
	}
	m_InfView.ShowCheckProblems(ranges);
	if (errors == 0 && warnings == 0)
		m_ProblemsText = L"No problems";
	else if (warnings == 0)
		m_ProblemsText = std::format(L"{} error{}", errors, errors == 1 ? L"" : L"s");
	else if (errors == 0)
		m_ProblemsText = std::format(L"{} warning{}", warnings, warnings == 1 ? L"" : L"s");
	else
		m_ProblemsText = std::format(L"{} err, {} warn", errors, warnings);
	if (IsWindowVisible())
		Frame()->SetStatusText(3, m_ProblemsText.c_str());
}

void CMainView::SetReadOnly(bool readOnly) {
	m_ReadOnly = readOnly;
	m_InfView.Execute(SCI_SETREADONLY, readOnly);
	UpdateUI();
}

LRESULT CMainView::OnToggleReadOnly(WORD, WORD, HWND, BOOL&) {
	SetReadOnly(!m_ReadOnly);
	return 0;
}
