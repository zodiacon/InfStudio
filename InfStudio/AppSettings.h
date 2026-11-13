#pragma once

#include "Settings.h"

class AppSettings : public Settings {
public:
	static constexpr PCWSTR RegistryKey = L"SOFTWARE\\ScorpioSoftware\\InfStudio";

	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
		SETTING(AlwaysOnTop, false, SettingType::Bool);
		SETTING(ViewToolBar, 1, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(TreeIconSize, 0, SettingType::Int32);
		SETTING(DarkMode, -1, SettingType::Int32);		// 1 dark, 0 light, -1 (never chosen): as the system is
	END_SETTINGS

	DEF_SETTING(AlwaysOnTop, bool)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(Font, LOGFONT)
	DEF_SETTING(TreeIconSize, int)
	DEF_SETTING(DarkMode, int)
	DEF_SETTING(ViewToolBar, int)
	DEF_SETTING(ViewStatusBar, int)
	DEF_SETTING_MULTI(RecentFiles)
};
