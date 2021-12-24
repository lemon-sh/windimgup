#include <windows.h>
#include <WinInet.h>
#include <string>

const DWORD errUnknown = 0x20000001;

const char* acceptTypes[] = { "application/json", NULL };
const char fdHeadBegin[] = "------974767299852498929531610575\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
const char fdHeadEnd[] = "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
const char fdEnd[] = "\r\n------974767299852498929531610575--\r\n";

extern "C" {
	DWORD uploadWebhook(const char* webhook, const char* data, DWORD dataLength, char* filename, char** url, void (*cbProgress)(DWORD sent, DWORD full), BOOL filenameAlloc);
}

/*
*  Wrapper over InternetWriteFile that ensures all bytes are written.
*  The parameters are the same as in InternetWriteFile, except for lpdwNumberOfBytesWritten,
*  which is nonexistent.
*/
bool writeInternetExact(HINTERNET req, const void* data, DWORD dataLength)
{
	const BYTE* pData = (const BYTE*)data;
	DWORD bytesSent;
	while (dataLength > 0)
	{
		if (!InternetWriteFile(req, pData, dataLength, &bytesSent)) return false;
		pData += bytesSent;
		dataLength -= bytesSent;
	}
	return true;
}

/*
*  Upload file from a buffer to a Discord webhook
*  -  [in] webhook: Path of the Discord webhook (e.g. "/api/webhooks/35467...")
*  -  [in] data: Buffer with the data that will be sent
*  -  [in] dataLength: Length of the buffer supplied as 'data'
*  -  [in] filename: Name of the uploaded file
*  - [out] url: URL of the uploaded file. Heap allocated: should be freed after use.
*  -  [in] cbProgress: Function that will be called every time data is sent.
*  Returns 0 on success and GetLastError() value on failure.
*/
DWORD uploadWebhook(const char* webhook, const char* data, DWORD dataLength, char* filename, char** url, void (*cbProgress)(DWORD sent, DWORD full), BOOL filenameAlloc) {
	if (!url) return errUnknown;
	HINTERNET wininet = 0, conn = 0, req = 0;
	INTERNET_BUFFERSA bufs{};
	char queryText[16] = { 0 };
	DWORD filenameLen, bytesWritten = 0, lastWrite = 0, queryTextSize = sizeof(queryText);
	size_t urlPos, urlEndPos, urlLength;

	std::string recvBuf;
	recvBuf.reserve(1024);

	wininet = InternetOpenA("WinDimgup/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
	if (wininet == 0) goto rip;
	conn = InternetConnectA(wininet, "discord.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, NULL, 0);
	if (conn == 0) goto rip;
	req = HttpOpenRequestA(conn, "POST", webhook, "HTTP/1.1", NULL, acceptTypes, INTERNET_FLAG_SECURE, 0);
	if (req == 0) goto rip;
	if (!HttpAddRequestHeadersA(req, "Content-Type: multipart/form-data; boundary=----974767299852498929531610575", 75, HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD)) goto rip;

	bufs.dwStructSize = sizeof(INTERNET_BUFFERS);
	filenameLen = (DWORD)strlen(filename);

	bufs.dwBufferTotal = (sizeof(fdHeadBegin) + sizeof(fdHeadEnd) + sizeof(fdEnd) - 3) + filenameLen + dataLength;

	if (!HttpSendRequestExA(req, &bufs, NULL, 0, 0)) goto rip;

	// writing head
	if (!writeInternetExact(req, fdHeadBegin, sizeof(fdHeadBegin) - 1)) goto rip;
	if (!writeInternetExact(req, filename, filenameLen)) goto rip;
	if (!writeInternetExact(req, fdHeadEnd, sizeof(fdHeadEnd) - 1)) goto rip;

	// writing data
	while (bytesWritten < dataLength) {
		if (!InternetWriteFile(req, data + bytesWritten, min(dataLength - bytesWritten, SHRT_MAX), &lastWrite)) goto rip;
		bytesWritten += lastWrite;
		cbProgress(bytesWritten, dataLength);
	}

	// writing end
	if (!writeInternetExact(req, fdEnd, sizeof(fdEnd) - 1)) goto rip;
	if (!HttpEndRequestA(req, NULL, 0, 0)) goto rip;

	while (true)
	{
		char tmp[4096];
		DWORD bytesRead;
		if (!InternetReadFile(req, tmp, sizeof(tmp), &bytesRead)) goto rip;
		if (bytesRead == 0) break;
		recvBuf.append(tmp, bytesRead);
	}
	urlPos = recvBuf.find("\"url\": \"");
	if (urlPos == std::string::npos) {
		SetLastError(errUnknown);
		*url = _strdup(recvBuf.c_str());
		goto rip;
	}
	urlEndPos = recvBuf.find('"', urlPos + 8);
	if (urlEndPos == std::string::npos) {
		SetLastError(errUnknown);
		*url = _strdup(recvBuf.c_str());
		goto rip;
	}
	urlLength = urlEndPos - urlPos - 8;
	*url = (char*)malloc(urlLength + 1);
	if (!*url) {
		SetLastError(errUnknown);
		goto rip;
	}
	memcpy(*url, recvBuf.c_str() + urlPos + 8, urlLength);
	(*url)[urlLength] = 0;
	SetLastError(ERROR_SUCCESS);
rip:
	DWORD status = GetLastError();
	if (req != 0) InternetCloseHandle(req);
	if (conn != 0) InternetCloseHandle(conn);
	if (wininet != 0) InternetCloseHandle(wininet);
	if (filenameAlloc) free(filename);
	return status;
}
