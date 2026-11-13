#pragma once

#include "ViewBase.h"
#include <TreeViewHelper.h>
#include "InfView.h"
#include <CustomSplitterWindow.h>
#include "..\InfCore\InfFile.h"
#include "TextFile.h"

class CMainView : 
	public CViewBase<CMainView>,
	public CTreeViewHelper<CMainView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);
	bool OpenFile(PCWSTR path);
	// a new, unsaved file with this text (UTF-8); Save As suggests name
	void NewFromText(std::string const& text, PCWSTR name);
	void CreateNew();
	bool OnTreeDoubleClick(HWND, HTREEITEM hItem);
	void OnActivated(bool active);
	bool CanClose() override;

	BEGIN_MSG_MAP(CMainView)
		NOTIFY_CODE_HANDLER(SCN_UPDATEUI, OnModified)
		NOTIFY_CODE_HANDLER(TVN_KEYDOWN, OnTreeKeyDown)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnEditFind)
		MESSAGE_HANDLER(CFindReplaceDialog::GetFindReplaceMsg(), OnFind)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS, OnSaveAs)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
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
	int AddKnownDirective(HTREEITEM hParent, PCWSTR section, PCWSTR directive, TreeIconIndex icon, NodeType type);

	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnModified(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEditFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CTreeViewCtrl m_Tree;
	CCustomSplitterWindow m_Splitter;
	CInfView m_InfView;
	InfFile m_Inf;
	CString m_Path;
	CString m_Name;
	UINT m_ErrorLine;
	int m_TreeIconSize{ 16 };
	CFindReplaceDialog* m_fr{ nullptr };
	CString m_SearchText;
	TextEncoding m_Encoding{ TextEncoding::Ansi };
	bool m_Unsaved{ false };
};

