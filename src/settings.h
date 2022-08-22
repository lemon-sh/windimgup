#pragma once
#include <windows.h>
int initSettings();
DWORD storeSettings(BYTE opts, const char* webhook);
DWORD loadSettings(BYTE *opts, char** webhook);
