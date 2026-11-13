#include "pch.h"
#include "resource.h"
#include "TemplateDlg.h"
#include "AppSettings.h"

InfTemplate const& CTemplateDlg::Template() const {
	return InfTemplates::All()[m_Index];
}

LRESULT CTemplateDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CComboBox combo(GetDlgItem(IDC_TEMPLATE));
	for (auto const& t : InfTemplates::All()) {
		// (the menu's names, without their accelerators)
		CString name(t.Name);
		name.Remove(L'&');
		combo.AddString(name);
	}
	combo.SetCurSel((int)m_Index);

	// the preview where its placeholder is: read-only, in the editor's font and colors
	CWindow placeholder(GetDlgItem(IDC_PREVIEW));
	CRect rc;
	placeholder.GetWindowRect(&rc);
	ScreenToClient(&rc);
	m_Preview.Create(m_hWnd, rc, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
	m_Preview.SetWindowPos(placeholder, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	m_Preview.SetReadOnly(true);

	m_Values = InfTemplates::DefaultValues(Template());
	if (auto manufacturer = AppSettings::Get().TemplateManufacturer(); !manufacturer.empty())
		m_Values.Manufacturer = manufacturer;
	m_Filling = true;
	SetText(IDC_MANUFACTURER, m_Values.Manufacturer);
	SetText(IDC_VERSION, m_Values.Version);
	m_Filling = false;
	ShowTemplate(nullptr);

	Layout();
	CenterWindow(GetParent());
	// the driver's name, all of it selected: what's typed first
	CEdit name(GetDlgItem(IDC_DRIVERNAME));
	name.SetFocus();
	name.SetSel(0, -1);
	return FALSE;
}

void CTemplateDlg::ShowTemplate(InfTemplate const* previous) {
	auto const& t = Template();
	auto replace = [&](UINT id, PCWSTR oldDefault, PCWSTR newDefault) {
		auto text = Text(id);
		if (text.empty() || (oldDefault && text == oldDefault) || !previous)
			SetText(id, newDefault ? newDefault : L"");
	};
	m_Filling = true;
	replace(IDC_DRIVERNAME, previous ? previous->DriverName : nullptr, t.DriverName);
	replace(IDC_DESCRIPTION, previous ? previous->Description : nullptr, t.Description);
	// (a template that isn't a device's has none)
	if (t.HardwareId)
		replace(IDC_HARDWAREID, previous ? previous->HardwareId : nullptr, t.HardwareId);
	m_Filling = false;
	GetDlgItem(IDC_HARDWAREID).EnableWindow(t.HardwareId != nullptr);
	GetDlgItem(IDC_HARDWAREID_LABEL).EnableWindow(t.HardwareId != nullptr);
	Update();
}

std::wstring CTemplateDlg::Text(UINT id) const {
	CString text;
	GetDlgItemText(id, text);
	text.Trim();
	return (PCWSTR)text;
}

void CTemplateDlg::SetText(UINT id, std::wstring const& text) {
	SetDlgItemText(id, text.c_str());
}

void CTemplateDlg::Update() {
	auto const& t = Template();
	m_Values.DriverName = Text(IDC_DRIVERNAME);
	m_Values.Manufacturer = Text(IDC_MANUFACTURER);
	m_Values.Description = Text(IDC_DESCRIPTION);
	m_Values.HardwareId = t.HardwareId ? Text(IDC_HARDWAREID) : L"";
	m_Values.Version = Text(IDC_VERSION);

	auto problem = InfTemplates::Check(t, m_Values);
	SetDlgItemText(IDC_PROBLEM, problem ? problem->Text.c_str() : L"");
	GetDlgItem(IDOK).EnableWindow(!problem);

	// the INF it makes (with a problem too: what's typed so far), where the preview was scrolled to
	auto first = m_Preview.Execute(SCI_GETFIRSTVISIBLELINE);
	m_Preview.SetReadOnly(false);
	m_Preview.SetText(InfTemplates::Generate(t, m_Values).c_str());
	m_Preview.SetReadOnly(true);
	m_Preview.EmptyUndoBuffer();
	m_Preview.UpdateLineNumberWidth();
	m_Preview.Execute(SCI_SETFIRSTVISIBLELINE, first);
}

LRESULT CTemplateDlg::OnTemplateChanged(WORD, WORD, HWND, BOOL&) {
	auto const& previous = Template();
	auto index = CComboBox(GetDlgItem(IDC_TEMPLATE)).GetCurSel();
	if (index < 0 || (size_t)index == m_Index)
		return 0;
	m_Index = index;
	m_Preview.Execute(SCI_SETFIRSTVISIBLELINE, 0);
	ShowTemplate(&previous);
	return 0;
}

LRESULT CTemplateDlg::OnValueChanged(WORD, WORD id, HWND, BOOL&) {
	// (the fields only: the preview's SCEN_CHANGE is the same code, and Update sets its text)
	if (id != IDC_DRIVERNAME && id != IDC_MANUFACTURER && id != IDC_DESCRIPTION && id != IDC_HARDWAREID && id != IDC_VERSION)
		return 0;
	if (!m_Filling && m_Preview.IsWindow())
		Update();
	return 0;
}

LRESULT CTemplateDlg::OnOK(WORD, WORD wID, HWND, BOOL&) {
	Update();
	if (InfTemplates::Check(Template(), m_Values))
		return 0;
	AppSettings::Get().TemplateManufacturer(m_Values.Manufacturer);
	EndDialog(wID);
	return 0;
}

LRESULT CTemplateDlg::OnCancel(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

void CTemplateDlg::Layout() {
	if (!m_Preview.IsWindow())
		return;
	CRect client;
	GetClientRect(&client);
	// (dialog units: they follow the DPI)
	CRect units(7, 14, 50, 20);		// margin, button height, button width, preview's top
	MapDialogRect(&units);
	int margin = units.left, buttonHeight = units.top, buttonWidth = units.right, top = units.bottom;

	CWindow ok(GetDlgItem(IDOK)), cancel(GetDlgItem(IDCANCEL));
	int y = client.bottom - margin - buttonHeight;
	int x = client.right - margin - buttonWidth;
	cancel.SetWindowPos(nullptr, x, y, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	ok.SetWindowPos(nullptr, x - margin / 2 - buttonWidth, y, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);

	// the preview: from its label to the buttons, from where the fields end to the right
	CRect label;
	GetDlgItem(IDC_PREVIEW_LABEL).GetWindowRect(&label);
	ScreenToClient(&label);
	m_Preview.SetWindowPos(nullptr, label.left, top, std::max(0, (int)(client.right - margin - label.left)),
		std::max(0, (int)(y - margin - top)), SWP_NOZORDER | SWP_NOACTIVATE);
	// the problem: under the fields, to the bottom
	CWindow problem(GetDlgItem(IDC_PROBLEM));
	CRect rc;
	problem.GetWindowRect(&rc);
	ScreenToClient(&rc);
	problem.SetWindowPos(nullptr, 0, 0, rc.Width(), std::max(0, (int)(y - margin - rc.top)), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	Invalidate();
}

LRESULT CTemplateDlg::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	Layout();
	return 0;
}

LRESULT CTemplateDlg::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&) {
	CRect size(0, 0, 400, 200);
	MapDialogRect(&size);
	auto info = reinterpret_cast<MINMAXINFO*>(lParam);
	info->ptMinTrackSize = { size.right, size.bottom };
	return 0;
}

LRESULT CTemplateDlg::OnDpiChanged(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	// Windows scales the dialog's controls (and its font) itself: the preview and the buttons once it has
	PostMessage(LayoutMessage);
	bHandled = FALSE;
	return 0;
}

LRESULT CTemplateDlg::OnLayout(UINT, WPARAM, LPARAM, BOOL&) {
	Layout();
	return 0;
}
