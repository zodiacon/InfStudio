#pragma once

#include "Interfaces.h"

template<typename T>
class CViewBase : public CFrameWindowImpl<T, CWindow, CControlWinTraits> {
	using BaseFrame = CFrameWindowImpl<T, CWindow, CControlWinTraits>;
public:
	CViewBase(IMainFrame* frame) : m_pFrame(frame) {}

	void OnFinalMessage(HWND) override {
		delete this;
	}

	IMainFrame* Frame() {
		return m_pFrame;
	}

	BEGIN_MSG_MAP(CViewBase)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)


private:
	IMainFrame* m_pFrame;
};

