#pragma once

#include "ViewBase.h"
#include <TreeViewHelper.h>
#include "InfView.h"
#include "..\InfCore\InfFile.h"

class CMainView : 
	public CViewBase<CMainView>,
	public CTreeViewHelper<CMainView> {
public:
	using CViewBase::CViewBase;

	bool OpenFile(PCWSTR path);
	void CreateNew();
	bool OnTreeDoubleClick(HWND, HTREEITEM hItem);
	void OnActivated(bool active);

	BEGIN_MSG_MAP(CMainView)
		NOTIFY_CODE_HANDLER(SCN_UPDATEUI, OnModified)
		NOTIFY_CODE_HANDLER(TVN_KEYDOWN, OnTreeKeyDown)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnSave)
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
		Models,
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
	};

	enum class TreeIconIndex {
		InfFile,
		Error,
		Sections,
		Section,
		AddReg,
		DelReg,
		AddFiles,
		DelFiles,
		Version,
		Models,
		Model,
	};

	void UpdateUI();
	bool DoSave(PCWSTR path);
	void BuildTree();
	void AnalyzeAndBuild(HTREEITEM hRoot);

	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnModified(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CTreeViewCtrl m_Tree;
	CSplitterWindow m_Splitter;
	CInfView m_InfView;
	InfFile m_Inf;
	CString m_Path;
	CString m_Name;
	UINT m_ErrorLine;
	int m_TreeIconSize{ 16 };
};

