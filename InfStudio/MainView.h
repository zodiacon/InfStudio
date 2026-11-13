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

class CMainView : 
	public CViewBase<CMainView>,
	public CTreeViewHelper<CMainView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);
	bool OpenFile(PCWSTR path);
	// a new, unsaved file with this text (UTF-8); Save As suggests name
	void NewFromText(std::string const& text, PCWSTR name);
	bool OnTreeDoubleClick(HWND, HTREEITEM hItem);
	void OnActivated(bool active);
	bool CanClose() override;

	BEGIN_MSG_MAP(CMainView)
		NOTIFY_CODE_HANDLER(SCN_UPDATEUI, OnModified)
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
	// changes not saved: edits, or a new file (from a template) that was never saved
	bool IsDirty() {
		return m_Unsaved || m_InfView.IsModified();
	}
	// an ANSI file with characters the code page doesn't have: asks whether to save it as Unicode. False if cancelled
	bool ConfirmEncoding();
	void BuildTree();
	void AnalyzeAndBuild(HTREEITEM hRoot);

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
	void Validate(PCWSTR mode);
	void ShowResults(std::vector<InfVerifMessage> messages, PCWSTR mode);
	void ShowResultsPane(bool show);
	LRESULT OnViewResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// a problem double-clicked (or Enter): its line in the editor
	LRESULT OnResultActivate(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	void GoToLine(int line);

	static constexpr UINT IDC_RESULTS = 1001;

	CTreeViewCtrl m_Tree;
	CCustomSplitterWindow m_Splitter;
	// the editor, with the problems InfVerif found under it
	CCustomHorSplitterWindow m_EditSplitter;
	CListViewCtrl m_Results;
	std::vector<InfVerifMessage> m_Messages;
	CInfView m_InfView;
	InfFile m_Inf;
	CString m_Path;
	CString m_Name;
	UINT m_ErrorLine;
	int m_TreeIconSize{ 16 };
	CFindReplaceDialog* m_fr{ nullptr };
	bool m_frReplace{ false };		// m_fr is a Replace dialog
	CString m_SearchText, m_ReplaceText;
	SearchOptions m_Search;
	TextEncoding m_Encoding{ TextEncoding::Ansi };
	bool m_Unsaved{ false };
};

