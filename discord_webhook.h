#pragma once
#include <windows.h>

DWORD uploadWebhook(const char* webhook, const char* data, DWORD dataLength, char* filename, char** url, void (*cbProgress)(DWORD sent, DWORD full));