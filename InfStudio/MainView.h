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

	BEGIN_MSG_MAP(CMainView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CTreeViewHelper<CMainView>)
		CHAIN_MSG_MAP(CViewBase)
	END_MSG_MAP()
	
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CTreeViewCtrl m_Tree;
	CSplitterWindow m_Splitter;
	CInfView m_InfView;
	InfFile m_Inf;
	UINT m_ErrorLine;
};

