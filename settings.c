#include <windows.h>

const DWORD errUnknown = 0x20000001;

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
	memcpy(configPath + fullSize, configFilename, configFilenameLength);
	return 0;
}

/*
*  Stores settings in the config file.
*  - [in] opts: Auxiliary data stored with the webhook
*  - [in] webhook: Discord webhook (as passed to uploadWebhook)
*  Returns 0 on success and GetLastError() value on failure.
*/
int storeSettings(BYTE opts, const char* webhook) {
	DWORD bytesWritten = 0, status = 0;
	size_t webhookLength = strlen(webhook), bufSize = webhookLength + 2;
	char* buf = (char*)malloc(bufSize);
	if (buf == NULL) return errUnknown;
	*buf = opts;
	memcpy(buf + 1, webhook, webhookLength + 1);
	HANDLE configFile = CreateFileW(configPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (configFile == INVALID_HANDLE_VALUE) goto rip;
	if (!WriteFile(configFile, buf, bufSize, &bytesWritten, NULL)) goto rip;
	if (bytesWritten != bufSize) { SetLastError(errUnknown); goto rip; }
	SetLastError(0);
rip:
	status = GetLastError();
	free(buf);
	if (configFile != INVALID_HANDLE_VALUE) CloseHandle(configFile);
	return status;
}