#pragma once

#include "Interfaces.h"

template<typename T>
class CViewBase : 
	public IView,
	public CFrameWindowImpl<T, CWindow, CControlWinTraits> {
	using BaseFrame = CFrameWindowImpl<T, CWindow, CControlWinTraits>;
public:
	CViewBase(IMainFrame* frame) : m_pFrame(frame) {}

	void OnFinalMessage(HWND) override {
		delete this;
	}

	IMainFrame* Frame() {
		return m_pFrame;
	}

	CUpdateUIBase& UI() {
		return m_pFrame->GetUI();
	}

	void PageActivated(bool activate) {
		static_cast<T*>(this)->OnActivated(activate);
	}

	BEGIN_MSG_MAP(CViewBase)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMessage)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:

	LRESULT OnForwardMessage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
		auto msg = reinterpret_cast<MSG*>(lParam);
		if (static_cast<T*>(this)->PreTranslateMessage(msg))
			return TRUE;
		return FALSE;
	}

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	void OnActivated(bool) {}

private:
	IMainFrame* m_pFrame;
};

