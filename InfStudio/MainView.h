#pragma once

#include "resource.h"
#include "ViewBase.h"
#include <TreeViewHelper.h>
#include "InfView.h"
#include <CustomSplitterWindow.h>
#include "..\InfCore\InfFile.h"
#include "TextFile.h"
#include "EditorSearch.h"
#include "InfVerif.h"
#include "InfNavigation.h"
#include "InfCompletion.h"
#include "InfChecks.h"
#include "InfLexer.h"
#include "AppSettings.h"
#include "DpiAware.h"
#include "..\InfCore\InfAnalysis.h"
#include <set>
#include <filesystem>
#include <thread>

class CMainView : 
	public CViewBase<CMainView>,
	public CTreeViewHelper<CMainView>,
	public CDpiAware<CMainView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);
	bool OpenFile(PCWSTR path);
	// a new, unsaved file with this text (UTF-8); Save As suggests name
	void NewFromText(std::string const& text, PCWSTR name);
	bool OnTreeDoubleClick(HWND, HTREEITEM hItem);
	void OnActivated(bool active);
	bool CanClose() override;
	void FontChanged() override;
	void CheckFileChanged() override;
	~CMainView();
	void LiveAnalysisChanged() override;

	BEGIN_MSG_MAP(CMainView)
		NOTIFY_CODE_HANDLER(SCN_UPDATEUI, OnModified)
		NOTIFY_CODE_HANDLER(SCN_MODIFIED, OnTextModified)
		NOTIFY_CODE_HANDLER(SCN_DWELLSTART, OnDwellStart)
		NOTIFY_CODE_HANDLER(SCN_CHARADDED, OnCharAdded)
		NOTIFY_CODE_HANDLER(SCN_AUTOCCOMPLETED, OnAutoCompleted)
		COMMAND_ID_HANDLER(ID_EDIT_COMPLETE, OnComplete)
		COMMAND_RANGE_HANDLER(ID_ENCODING_ANSI, ID_ENCODING_UTF16, OnEncoding)
		COMMAND_ID_HANDLER(ID_FILE_READONLY, OnToggleReadOnly)
		NOTIFY_CODE_HANDLER(SCN_DWELLEND, OnDwellEnd)
		COMMAND_ID_HANDLER(ID_EDIT_GOTO_DEFINITION, OnGoToDefinition)
		COMMAND_ID_HANDLER(ID_EDIT_GO_BACK, OnGoBack)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(ValidatedMessage, OnValidated)
		MESSAGE_HANDLER(CheckFileMessage, OnCheckFile)
		NOTIFY_CODE_HANDLER(TVN_KEYDOWN, OnTreeKeyDown)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnEditFind)
		COMMAND_ID_HANDLER(ID_EDIT_REPLACE, OnEditReplace)
		COMMAND_ID_HANDLER(ID_EDIT_REPEAT, OnFindNext)
		COMMAND_ID_HANDLER(ID_EDIT_FIND_PREVIOUS, OnFindNext)
		MESSAGE_HANDLER(CFindReplaceDialog::GetFindReplaceMsg(), OnFind)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS, OnSaveAs)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_INF_VALIDATE, OnValidate)
		COMMAND_ID_HANDLER(ID_INF_VALIDATE_WHQL, OnValidate)
		COMMAND_ID_HANDLER(ID_VIEW_RESULTS, OnViewResults)
		NOTIFY_HANDLER(IDC_RESULTS, NM_DBLCLK, OnResultActivate)
		NOTIFY_HANDLER(IDC_RESULTS, NM_RETURN, OnResultActivate)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CDpiAware<CMainView>)
		CHAIN_MSG_MAP_MEMBER(m_InfView)
		CHAIN_MSG_MAP(CTreeViewHelper<CMainView>)
		CHAIN_MSG_MAP(CViewBase)
	END_MSG_MAP()
	
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class NodeType {
		Invalid,
		Root,
		Sections,
		Error,

		Section = 0x100,
		Version,
		AddReg,
		DelReg,
		CopyFiles,
		Model,
		DelFiles,
		DestinationDirs,
		Strings,
		Models,
	};

	enum class TreeIconIndex {
		InfFile,
		Error,
		Sections,
		Section,
		AddReg,
		DelReg,
		CopyFiles,
		DelFiles,
		Version,
		Models,
		Model,
	};

	void DisplaySaveError();
	void UpdateUI();
	bool DoSave(PCWSTR path);
	// changes not saved: edits, a new file (from a template) that was never saved, or another encoding (m_Unsaved)
	bool IsDirty() {
		return m_Unsaved || m_InfView.IsModified();
	}
	// an ANSI file with characters the code page doesn't have: asks whether to save it as Unicode. False if cancelled
	bool ConfirmEncoding();
	// The tree of the editor's text (saved or not): its analysis (AnalyzeInf) of a copy in a temporary file,
	// with what was expanded and selected kept (by the items' texts). Again after an edit, once typing pauses.
	void BuildTree();
	void AddNodes(HTREEITEM hParent, std::vector<InfNode> const& nodes);
	struct TreeState {
		std::set<std::vector<std::wstring>> Expanded;
		std::vector<std::wstring> Selected;
	};
	std::vector<std::wstring> TreePath(HTREEITEM hItem) const;
	void SaveTreeState(HTREEITEM hItem, TreeState& state) const;
	void RestoreTreeState(HTREEITEM hItem, TreeState const& state);
	// The editor's text in a file of its own (by the view's name, in a temporary folder of the view, one for each purpose:
	// the analysis deleting its copy mustn't take the one InfVerif is reading), for SetupAPI and InfVerif.
	std::filesystem::path WriteTempCopy(std::wstring const& purpose);
	static void DeleteTempCopy(std::filesystem::path const& file);
	static constexpr UINT_PTR AnalysisTimer = 1;
	static constexpr UINT AnalysisDelay = 500;
	LRESULT OnTextModified(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnModified(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEditFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditReplace(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// F3 (ID_EDIT_REPEAT) and Shift+F3 (ID_EDIT_FIND_PREVIOUS): the last search again, down or up
	LRESULT OnFindNext(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the Find (or Replace) dialog, with the selection as what to find
	void ShowFindDialog(bool replace);
	// the selected text, if it is on one line (what to find), else empty
	CString SelectedText();
	LRESULT OnFind(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// INF > Validate (/w) and Validate for WHQL (/h): InfVerif on the editor's text, its problems in the pane under it
	LRESULT OnValidate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// InfVerif runs on a thread (m_Validator): the results come in ValidatedMessage. Validating again (or the view going)
	// stops a run still going (its process killed); the results of one that isn't the latest (m_ValidationGeneration) are dropped.
	void Validate(PCWSTR mode);
	struct ValidationDone {
		UINT Generation;
		std::wstring Mode;
		InfVerifResult Result;
		size_t TextVersion;		// m_TextVersion when it started
		std::wstring Copy;		// the temporary copy InfVerif read (its messages name it)
	};
	static constexpr UINT ValidatedMessage = WM_APP + 10;
	LRESULT OnValidated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void ShowRunning(PCWSTR mode);
	// the results aren't of the text there now: a row first that says so (once)
	void MarkResultsStale();
	void ShowResults(std::vector<InfVerifMessage> messages, PCWSTR mode);
	void ShowResultsPane(bool show);
	LRESULT OnViewResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// a problem double-clicked (or Enter): its line in the editor
	LRESULT OnResultActivate(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	void GoToLine(int line);

	// Go to Definition (F12, Ctrl+click): what's at pos (InfNavigation::FindDefinition); false if there's nothing to go to.
	// Go Back (Alt+Left) returns to where it was.
	bool GoToDefinition(LRESULT pos);
	LRESULT OnGoToDefinition(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGoBack(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// hovering a string key: its value in a tip
	LRESULT OnDwellStart(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnDwellEnd(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	// Auto-complete (InfCompletion): shown as a string key, a section header or a word starts (SCN_CHARADDED), and on
	// Ctrl+Space; the item chosen gets its suffix ('%', ']', " = ") after it, and a key whose value names sections the list
	// of them next.
	void ShowCompletion(bool asked);
	LRESULT OnCharAdded(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnAutoCompleted(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnComplete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// File > Encoding: what the file is saved as (TextEncoding's order); a change is unsaved until the next save
	LRESULT OnEncoding(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// Read-only (a file of Windows', or one that can't be written: TextFile::ShouldOpenReadOnly; File > Read Only turns it
	// off or on): the editor takes no edits and Save is off; Save As saves a copy, which is the view's file then.
	void SetReadOnly(bool readOnly);
	LRESULT OnToggleReadOnly(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	bool m_ReadOnly{ false };
	std::string m_CompletionSuffix;

	// The built-in checks (InfChecks) of the editor's text, run with the tree's analysis: squiggles, their messages when
	// hovered, and their count in the status bar.
	void RunChecks(std::string const& text);
	std::vector<InfProblem> m_CheckProblems;
	std::wstring m_ProblemsText;

	// the tree's selection follows the caret: the section it is in (unless the selected item has that name already)
	void FollowCaret();
	std::wstring SectionAtCaret();

	// Per-monitor DPI (CDpiAware, after the controls have handled the change): the tree's and the results' font and icons,
	// and the editor's margins
	friend class CDpiAware<CMainView>;
	void OnDpiChanged(UINT dpi);
	void ApplyDpi();
	CFont m_UiFont;
	CImageList m_TreeImages, m_ResultImages;

	static constexpr UINT IDC_RESULTS = 1001;

	CTreeViewCtrl m_Tree;
	CCustomSplitterWindow m_Splitter;
	// the editor, with the problems InfVerif found under it
	CCustomHorSplitterWindow m_EditSplitter;
	CListViewCtrl m_Results;
	std::vector<InfVerifMessage> m_Messages;
	std::jthread m_Validator;
	UINT m_ValidationGeneration{ 0 };
	size_t m_TextVersion{ 0 };			// counts the edits
	bool m_ResultsShown{ false }, m_ResultsStale{ false };

	// The file changed on disk (by another program; seen when the app or the tab is active again: CheckFileMessage): an
	// unmodified file is reloaded, a modified one only if the user says so; a file that's gone is told of.
	static constexpr UINT CheckFileMessage = WM_APP + 11;
	LRESULT OnCheckFile(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void Reload();
	std::optional<FileStamp> m_Stamp;	// the file as it was opened or saved
	bool m_CheckingFile{ false }, m_FileGone{ false };
	CInfView m_InfView;
	CString m_Path;
	CString m_Name;
	CFindReplaceDialog* m_fr{ nullptr };
	bool m_frReplace{ false };		// m_fr is a Replace dialog
	CString m_SearchText, m_ReplaceText;
	SearchOptions m_Search;
	std::vector<LRESULT> m_BackPositions;
	std::wstring m_CaretSection;
	TextEncoding m_Encoding{ TextEncoding::Ansi };
	bool m_Unsaved{ false };
};

