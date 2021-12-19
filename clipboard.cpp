#include <windows.h>
#include <gdiplus.h>

extern "C" {
	int getPngFromClipboard(HGLOBAL* hgl);
}

int getEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	UINT num = 0;
	UINT size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL) return -1;
	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j) {
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return 0;
		}
	}
	free(pImageCodecInfo);
	return -1;
}

int getPngFromClipboard(HGLOBAL* hgl) {
	int status = 0;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	CLSID encoderClsid;
	HGLOBAL imgMem;
	IStream* stream{};
	void* bmpBits;
	LPBITMAPINFO bmpInfo{};

	if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Status::Ok) { status = 1; goto rip; }
	if (getEncoderClsid(L"image/png", &encoderClsid)) { status = 2; goto rip; }

	if (!OpenClipboard(NULL)) { status = 3; goto rip; }
	imgMem = GetClipboardData(CF_DIB);
	if (imgMem == NULL) { status = 4; goto rip; }
	bmpInfo = (LPBITMAPINFO)GlobalLock(imgMem);
	bmpBits = (void*)(bmpInfo + 1);

	if (FAILED(CreateStreamOnHGlobal(NULL, FALSE, &stream))) { status = 5; goto rip; }

	{
		Gdiplus::Bitmap bmp(bmpInfo, bmpBits);
		bmp.Save(stream, &encoderClsid);
	}

	if (FAILED(GetHGlobalFromStream(stream, hgl))) { status = 6; }
rip:
	if (bmpInfo != NULL) GlobalUnlock(bmpInfo);
	if (stream != NULL) stream->Release();
	CloseClipboard();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return status;
}
