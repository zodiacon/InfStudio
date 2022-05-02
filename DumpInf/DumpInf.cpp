// DumpInf.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "..\InfCore\InfFile.h"

int wmain(int argc, wchar_t const* argv[]) {
	InfFile inf;
	bool ok;
	if (argc < 2) {
		printf("Opening master INF...\n");
		ok = inf.OpenMaster();
	}
	else {
		printf("Opening %ws...\n", argv[1]);
		ok = inf.Open(argv[1]);
	}

	if (!ok) {
		printf("Failed to open file\n");
		return 1;
	}

	int count = inf.GetFileCount();
	printf("File count: %d\n", count);

	for (int i = 0; i < count; ++i) {
		printf("Version info: \n");
		for (auto& [key, value] : inf.GetVersionSectionInfo(nullptr, i)) {
			printf("%ws = %ws\n", key.c_str(), value.c_str());
		}

		printf("Sections:\n");
		for (auto const& sec : inf.GetSectionNames()) {
			printf("\t%ws\n", sec.c_str());
			//for (auto& [key, text] : inf.GetSectionCompactLines(sec.c_str())) {
			//	if (key.empty())
			//		printf("\t\t%ws\n", text.c_str());
			//	else
			//		printf("\t\t%ws = %ws\n", key.c_str(), text.c_str());
			//}
		}

		//printf("Sections (original text):\n");
		//for (auto const& sec : inf.GetSectionNames()) {
		//	printf("\t%ws\n", sec.c_str());
		//	for (auto& [key, text] : inf.GetSectionLines(sec.c_str())) {
		//		if (key.empty())
		//			printf("\t\t%ws\n", text.c_str());
		//		else
		//			printf("\t\t%ws = %ws\n", key.c_str(), text.c_str());
		//	}
		//}
	}

	inf.Close();
	return 0;
}
