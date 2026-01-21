#include <windows.h>
#include <stdio.h>
#include "app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd){

	char romPath[MAX_PATH] = { 0 };

	if (lpCmdLine && lpCmdLine[0])
	{
		// Remove quotes if present
		if (lpCmdLine[0] == '"') {
			char* end = strrchr(lpCmdLine + 1, '"');
			if (end) *end = '\0';
			strcpy_s(romPath, MAX_PATH, lpCmdLine + 1);
		}
		else {
			strcpy_s(romPath, MAX_PATH, lpCmdLine);
		}
	}
	else{
		wchar_t outPath[MAX_PATH];
		OPENFILENAMEW ofn = { 0 };
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = L"NES ROM (*.nes)\0*.nes\0All Files\0*.*\0";
		ofn.lpstrFile = outPath;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		outPath[0] = L'\0';
		if (!GetOpenFileNameW(&ofn)) return 0;

		wcstombs_s(NULL, romPath, sizeof(romPath), outPath, sizeof(outPath));
	}

	if (!initialise_app(romPath)) return -1;

	int ecode = 0;
	while (is_running()){
		ecode = run();
	}

	deinitialise_app();
	return ecode;
}