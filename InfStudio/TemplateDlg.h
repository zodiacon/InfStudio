#pragma once

#include "InfTemplates.h"
#include "InfView.h"

//
// File > New from Template: the template wizard. It asks for what the template has defaults for (the driver's name,
// the manufacturer, the description, the hardware ID, the version), shows the INF it makes as they are typed, and won't
// make it while a value isn't good (InfTemplates::Check). The manufacturer is remembered (AppSettings::TemplateManufacturer).
//
class CTemplateDlg : public CDialogImpl<CTemplateDlg> {
public:
	enum { IDD = IDD_TEMPLATE };

	explicit CTemplateDlg(size_t templateIndex) : m_Index(templateIndex) {}

	// what was chosen, after IDOK
	InfTemplate const& Template() const;
	InfTemplates::Values const& Values() const {
		return m_Values;
	}

	BEGIN_MSG_MAP(CTemplateDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(LayoutMessage, OnLayout)
		COMMAND_HANDLER(IDC_TEMPLATE, CBN_SELCHANGE, OnTemplateChanged)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnValueChanged)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	static constexpr UINT LayoutMessage = WM_APP + 1;

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDpiChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLayout(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTemplateChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnValueChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// the preview and the buttons follow the dialog's size (the fields stay where they are)
	void Layout();
	// the template's defaults in the fields; a value the user typed stays (one that's still the old template's default doesn't)
	void ShowTemplate(InfTemplate const* previous);
	// the fields to m_Values, the preview, the problem (if any) and Create
	void Update();
	std::wstring Text(UINT id) const;
	void SetText(UINT id, std::wstring const& text);

	size_t m_Index;
	InfTemplates::Values m_Values;	// (the date and GUID made once: the preview doesn't change them as it's typed)
	CInfView m_Preview;
	bool m_Filling{ false };		// the fields being set (no Update for each)
};
