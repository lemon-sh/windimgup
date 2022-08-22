#pragma once

#include <windows.h>

DWORD uploadWebhook(const char *webhook, const char *data, DWORD dataLength, const char *filename, char **url,
                    void (*cbProgress)(DWORD sent, DWORD full));