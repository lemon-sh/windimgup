#include <windows.h>

WCHAR configPath[512];
const DWORD configPathSize = sizeof(configPath) / sizeof(WCHAR);

const WCHAR configFilename[] = L"\\windimgup.wdp";
const DWORD configFilenameLength = sizeof(configFilename) / sizeof(WCHAR);

/*
*  Initializes settings. Must be called before any other functions in this module.
*  Returns 0 on success, 1 when the config path is too long.
*/
int initSettings() {
    DWORD fullSize = GetEnvironmentVariableW(L"USERPROFILE", configPath, configPathSize);
    if (fullSize + configFilenameLength + 1 > configPathSize) {
        return 1;
    }
    memcpy(configPath + fullSize, configFilename, configFilenameLength * sizeof(WCHAR));
    return 0;
}

/*
*  Stores settings in the config file.
*  - [in] opts: Auxiliary data stored with the webhook
*  - [in] webhook: Discord webhook (as passed to uploadWebhook)
*  Returns 0 on success and GetLastError() value on failure.
*/
DWORD storeSettings(BYTE opts, const char *webhook) {
    DWORD bytesWritten = 0, status;
    size_t webhookLength = strlen(webhook);
    DWORD bufSize = (DWORD) webhookLength + 2;
    BYTE *buf = malloc(bufSize);
    if (buf == NULL) return ERROR_NOT_ENOUGH_MEMORY;
    *buf = opts;
    memcpy(buf + 1, webhook, webhookLength + 1);
    HANDLE configFile = CreateFileW(configPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (configFile == INVALID_HANDLE_VALUE) goto rip;
    if (!WriteFile(configFile, buf, bufSize, &bytesWritten, NULL)) goto rip;
    if (bytesWritten != bufSize) {
        SetLastError(ERROR_WRITE_FAULT);
        goto rip;
    }
    SetLastError(0);
    rip:
    status = GetLastError();
    free(buf);
    if (configFile != INVALID_HANDLE_VALUE) CloseHandle(configFile);
    return status;
}

/*
*  Loads settings from the config file.
*  - [out] opts: Auxiliary data stored with the webhook
*  - [out] webhook: Discord webhook (as passed to uploadWebhook)
*  Returns 0 on success and GetLastError() value on failure.
*/
DWORD loadSettings(BYTE *opts, char **webhook) {
    LARGE_INTEGER filesize;
    DWORD status, bytesRead;
    *webhook = 0;

    HANDLE file = CreateFileW(configPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if (file == INVALID_HANDLE_VALUE) goto rip;
    if (!GetFileSizeEx(file, &filesize)) goto rip;
    LONGLONG webhooklen = filesize.QuadPart - 1;

    if (webhooklen < 1) {
        SetLastError(ERROR_INVALID_DATA);
        goto rip;
    }
    *webhook = (char *) malloc((size_t) webhooklen);
    if (!*webhook) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto rip;
    }
    if (!ReadFile(file, opts, 1, &bytesRead, NULL)) goto rip;
    if (bytesRead != 1) {
        SetLastError(ERROR_READ_FAULT);
        goto rip;
    }
    if (!ReadFile(file, *webhook, (DWORD) webhooklen, &bytesRead, NULL)) goto rip;
    if (bytesRead != webhooklen) {
        SetLastError(ERROR_READ_FAULT);
        goto rip;
    }
    (*webhook)[webhooklen - 1] = 0;
    SetLastError(0);
    rip:
    status = GetLastError();
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (*webhook && status) free(webhook);
    return status;
}