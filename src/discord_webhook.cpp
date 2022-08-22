#include <windows.h>
#include <WinInet.h>
#include <string>

#define lemin(a, b) ((a) < (b) ? (a) : (b))

const char *acceptTypes[] = {"application/json", nullptr};
const char fdHeadBegin[] = "------974767299852498929531610575\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
const char fdHeadEnd[] = "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
const char fdEnd[] = "\r\n------974767299852498929531610575--\r\n";

extern "C" {
DWORD uploadWebhook(const char *webhook, const char *data, DWORD dataLength, const char *filename, char **url,
                    void (*cbProgress)(DWORD sent, DWORD full));
}

/*
*  Wrapper over InternetWriteFile that ensures all bytes are written.
*  The parameters are the same as in InternetWriteFile, except for lpdwNumberOfBytesWritten,
*  which is nonexistent.
*/
bool writeInternetExact(HINTERNET req, const void *data, DWORD dataLength) {
    const BYTE *pData = (const BYTE *) data;
    DWORD bytesSent;
    while (dataLength > 0) {
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
DWORD uploadWebhook(const char *webhook, const char *data, DWORD dataLength, const char *filename, char **url,
                    void (*cbProgress)(DWORD sent, DWORD full)) {
    if (!*filename) filename = "wdmupload.png";
    HINTERNET wininet, conn = nullptr, req = nullptr;
    INTERNET_BUFFERSA bufs{};
    DWORD filenameLen, bytesWritten = 0, lastWrite = 0;
    size_t urlPos, urlEndPos, urlLength;

    std::string recvBuf;
    recvBuf.reserve(1024);

    wininet = InternetOpenA("WinDimgup/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, NULL);
    if (wininet == nullptr) goto rip;
    conn = InternetConnectA(wininet, "discord.com", INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr,
                            INTERNET_SERVICE_HTTP, NULL, 0);
    if (conn == nullptr) goto rip;
    req = HttpOpenRequestA(conn, "POST", webhook, "HTTP/1.1", nullptr, acceptTypes, INTERNET_FLAG_SECURE, 0);
    if (req == nullptr) goto rip;
    if (!HttpAddRequestHeadersA(req, "Content-Type: multipart/form-data; boundary=----974767299852498929531610575", 75,
                                HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD))
        goto rip;

    bufs.dwStructSize = sizeof(INTERNET_BUFFERS);
    filenameLen = (DWORD) strlen(filename);

    bufs.dwBufferTotal = (sizeof(fdHeadBegin) + sizeof(fdHeadEnd) + sizeof(fdEnd) - 3) + filenameLen + dataLength;

    if (!HttpSendRequestExA(req, &bufs, nullptr, 0, 0)) goto rip;

    // writing head
    if (!writeInternetExact(req, fdHeadBegin, sizeof(fdHeadBegin) - 1)) goto rip;
    if (!writeInternetExact(req, filename, filenameLen)) goto rip;
    if (!writeInternetExact(req, fdHeadEnd, sizeof(fdHeadEnd) - 1)) goto rip;

    // writing data
    while (bytesWritten < dataLength) {
        if (!InternetWriteFile(req, data + bytesWritten, lemin(dataLength - bytesWritten, SHRT_MAX), &lastWrite))
            goto rip;
        bytesWritten += lastWrite;
        cbProgress(bytesWritten, dataLength);
    }

    // writing end
    if (!writeInternetExact(req, fdEnd, sizeof(fdEnd) - 1)) goto rip;
    if (!HttpEndRequestA(req, nullptr, 0, 0)) goto rip;

    while (true) {
        char tmp[4096];
        DWORD bytesRead;
        if (!InternetReadFile(req, tmp, sizeof(tmp), &bytesRead)) goto rip;
        if (bytesRead == 0) break;
        recvBuf.append(tmp, bytesRead);
    }
    urlPos = recvBuf.find(R"("url": ")");
    if (urlPos == std::string::npos) {
        SetLastError(ERROR_INVALID_DATA);
        *url = _strdup(recvBuf.c_str());
        goto rip;
    }
    urlEndPos = recvBuf.find('"', urlPos + 8);
    if (urlEndPos == std::string::npos) {
        SetLastError(ERROR_INVALID_DATA);
        *url = _strdup(recvBuf.c_str());
        goto rip;
    }
    urlLength = urlEndPos - urlPos - 8;
    *url = (char *) malloc(urlLength + 1);
    if (!*url) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto rip;
    }
    memcpy(*url, recvBuf.c_str() + urlPos + 8, urlLength);
    (*url)[urlLength] = 0;
    SetLastError(0);
    rip:
    DWORD status = GetLastError();
    if (req != nullptr) InternetCloseHandle(req);
    if (conn != nullptr) InternetCloseHandle(conn);
    if (wininet != nullptr) InternetCloseHandle(wininet);
    return status;
}
