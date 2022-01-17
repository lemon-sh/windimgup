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
	auto* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == nullptr) return -1;
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

/*
*  Retreives an image from the clipboard and encodes it as PNG
*  - [out] hgl: Memory object that contains the PNG
*  Returns 0 on success, custom error code on failure.
*/
int getPngFromClipboard(HGLOBAL* hgl) {
	int status = 0;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	CLSID encoderClsid;
	HGLOBAL imgMem;
	IStream* stream = nullptr;
	void* bmpBits;
	LPBITMAPINFO bmpInfo = nullptr;

	if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Status::Ok) { status = 1; goto rip; }
	if (getEncoderClsid(L"image/png", &encoderClsid)) { status = 2; goto rip; }

	if (!OpenClipboard(nullptr)) { status = 3; goto rip; }
	imgMem = GetClipboardData(CF_DIB);
	if (imgMem == nullptr) { status = 4; goto rip; }
	bmpInfo = (LPBITMAPINFO)GlobalLock(imgMem);
	bmpBits = (void*)(bmpInfo + 1);

	if (FAILED(CreateStreamOnHGlobal(nullptr, FALSE, &stream))) { status = 5; goto rip; }

	{
		Gdiplus::Bitmap bmp(bmpInfo, bmpBits);
		bmp.Save(stream, &encoderClsid, nullptr);
	}

	if (FAILED(GetHGlobalFromStream(stream, hgl))) { status = 6; }
rip:
	if (bmpInfo != nullptr) GlobalUnlock(bmpInfo);
	if (stream != nullptr) stream->Release();
	CloseClipboard();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return status;
}
