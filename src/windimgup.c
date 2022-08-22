#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <Commctrl.h>
#include <stdio.h>
#include <process.h>

#include "settings.h"
#include "discord_webhook.h"
#include "clipboard.h"

#define METASTRING(x) x,(sizeof(x)-1)

// ------------------------------------------------------------------------- //

// handles
HINSTANCE hInst;
HFONT uiFont, titleFont;
HBRUSH bgBrush, brightBrush;
HANDLE uploadThread;

// window size
#define WIN_WIDTH 300
#define WIN_HEIGHT 215

// colors
#define DCOLOR_BG_DARK   RGB(40,40,40)
#define DCOLOR_BG_BRIGHT RGB(60,60,60)

// controls
enum ControlClass {
	CT_TEXT, CT_EDIT, CT_CHECK, CT_UPCLIP, CT_WEBHOOK, CT_UPFILE, CT_PROGRESS, CT_COPY, CT_CAPTION
};
HWND webhookEdit, webhookButton, outputEdit, progressLabel, autocopyCheckbox, progressBar, fileButton, clipButton;
BOOL isInSettings;

// ------------------------------------------------------------------------- //

HWND createButton(HWND parent, UINT_PTR controlId, const char* label, int posX, int posY, int width, int height) {
	HWND btn = CreateWindowExA(0, "BUTTON", label, WS_VISIBLE | WS_CHILD,
		posX, posY, width, height, parent, (HMENU)controlId, hInst, 0);
	SendMessageA(btn, WM_SETFONT, (WPARAM)uiFont, MAKELPARAM(TRUE, 0));
	return btn;
}

HWND createEdit(HWND parent, UINT_PTR controlId, int posX, int posY, int width, int height, int flags) {
	HWND edit = CreateWindowExA(
		WS_EX_CLIENTEDGE, "EDIT", 0, WS_CHILD | WS_VISIBLE | ES_LEFT | flags,
		posX, posY, width, height, parent, (HMENU)controlId, hInst, 0);
	SendMessageA(edit, WM_SETFONT, (WPARAM)uiFont, MAKELPARAM(TRUE, 0));
	SendMessageA(edit, EM_SETLIMITTEXT, 10000, 0);
	return edit;
}

HWND createLabel(HWND parent, UINT_PTR controlId, const char* label, HFONT font, int posX, int posY, int width, int height) {
	HWND statictrl = CreateWindowExA(0, "STATIC", label, WS_CHILD | WS_VISIBLE, posX, posY, width, height, parent,
		(HMENU)controlId, hInst, 0);
	SendMessageA(statictrl, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
	return statictrl;
}

HWND createCheckbox(HWND parent, UINT_PTR controlId, int posX, int posY, int width, int height) {
	HWND checkbox = CreateWindowExA(0, "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_MULTILINE, posX, posY, width, height, parent, (HMENU)controlId, hInst, 0);
	SendMessageA(checkbox, WM_SETFONT, (WPARAM)uiFont, MAKELPARAM(TRUE, 0));
	return checkbox;
}

HWND createProgress(HWND parent, UINT_PTR controlId, int max, int posX, int posY, int width, int height) {
	HWND progressBarHwnd = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, posX, posY, width, height, parent, (HMENU)controlId, hInst, NULL);
	SendMessageA(progressBarHwnd, PBM_SETRANGE, 0, (LPARAM)MAKELONG(0, max));
	return progressBarHwnd;
}

// ------------------------------------------------------------------------- //

/*
*  Initialize fonts and brushes
*/
void initializeGdiObjects() {
	uiFont = CreateFontA(16, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, FF_DONTCARE, "Segoe UI");
	titleFont = CreateFontA(32, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, FF_DONTCARE, "Segoe UI");
	bgBrush = CreateSolidBrush(DCOLOR_BG_DARK);
	brightBrush = CreateSolidBrush(DCOLOR_BG_BRIGHT);
}

/*
*  Creates the UI
*/
void createControls(HWND hWnd) {
	createLabel(hWnd, CT_TEXT, "WinDimgup", titleFont, 10, 8, 130, 40);
	createLabel(hWnd, CT_CAPTION, "by ./lemon.sh", uiFont, 145, 25, 100, 40);

	createLabel(hWnd, CT_TEXT, "Upload:", uiFont, 12, 55, 50, 22);
	fileButton = createButton(hWnd, CT_UPFILE, "File", 60, 52, 60, 22);
	clipButton = createButton(hWnd, CT_UPCLIP, "Image from clipboard", 120, 52, 150, 22);
	progressLabel = createLabel(hWnd, CT_TEXT, "", uiFont, 12, 75, 50, 22);
	progressBar = createProgress(hWnd, CT_PROGRESS, 100, 61, 75, 208, 18);
	outputEdit = createEdit(hWnd, CT_EDIT, 12, 100, 200, 22, ES_READONLY | ES_AUTOHSCROLL);
	createButton(hWnd, CT_COPY, "Copy", 215, 100, 55, 22);

	webhookEdit = createEdit(hWnd, CT_EDIT, 12, 125, 200, 22, ES_AUTOHSCROLL);
	EnableWindow(webhookEdit, 0);
	webhookButton = createButton(hWnd, CT_WEBHOOK, "Edit", 215, 125, 55, 22);
	autocopyCheckbox = createCheckbox(hWnd, CT_CHECK, 12, 150, 15, 20);
	createLabel(hWnd, CT_TEXT, "Automatically copy link to clipboard", uiFont, 30, 152, 200, 20);
	EnableWindow(autocopyCheckbox, 0);

	SendMessageA(webhookEdit, EM_SETCUEBANNER, 1, (LPARAM)L"Webhook...");
}

/*
*  Display error box from an error code
*/
void errorMsgbox(DWORD errorCode)
{
	LPSTR lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
		GetModuleHandleA("wininet.dll"), errorCode, 0, (LPSTR)&lpMsgBuf, 0, NULL);
	MessageBoxA(NULL, lpMsgBuf, "Error", MB_ICONERROR);
	LocalFree(lpMsgBuf);
}

/*
*  Copy contents of the outputEdit control to clipboard
*/
void copyLinkToClipboard(HWND hwnd) {
	if (!OpenClipboard(hwnd)) return;
	SetLastError(0);
	int urlLen = GetWindowTextLengthA(outputEdit)+1;
	HGLOBAL urlMem = 0;
	DWORD status;
    if (urlLen == 0) {
		MessageBoxA(NULL, "There's nothing to copy.", "Error", MB_ICONINFORMATION);
		goto rip;
	}
	if (!(urlMem = GlobalAlloc(GMEM_FIXED, urlLen))) goto rip;
	if (!GetWindowTextA(outputEdit, urlMem, urlLen)) goto rip;
	if (!EmptyClipboard()) goto rip;
	if (!(SetClipboardData(CF_TEXT, urlMem))) goto rip;
	urlMem = 0;
rip:
	status = GetLastError();
	if (status) errorMsgbox(status);
	CloseClipboard();
	if (urlMem) GlobalFree(urlMem);
}

/*
*  Callback for the uploadWebhook function
*/
void updateProgress(DWORD a, DWORD b) {
	DWORD percent = a*100 / b;
	char progressText[8];
	snprintf(progressText, sizeof progressText, "%lu%%", percent);
	SendMessageA(progressBar, PBM_SETPOS, percent, 0);
	SetWindowTextA(progressLabel, progressText);
}

typedef struct {
	HGLOBAL data;
	HWND parent;
	char filename[256];
} uploadParam;

/*
*  Handles upload in a window context. Should be run in a thread (hence stdcall).
*/
DWORD __stdcall uploadHelper(uploadParam *param) {
	char webhook[256];
	char *webhook_start = webhook, *dataLocked = GlobalLock(param->data), *url = 0;
	SIZE_T dataSize = GlobalSize(param->data);
	DWORD errCode;

	if (dataSize == 0) goto rip;
	if (dataLocked == NULL) goto rip;
	EnableWindow(fileButton, FALSE);
	EnableWindow(clipButton, FALSE);
	if (!GetWindowTextA(webhookEdit, webhook, sizeof(webhook))) {
		MessageBoxA(NULL, "Empty webhook", "Error", MB_ICONERROR);
		goto rip;
	}
	if (!strncmp(webhook, METASTRING("https://discord.com"))) {
		webhook_start += sizeof("https://discord.com") - 1;
	}

	switch ((errCode = uploadWebhook(webhook_start, dataLocked, (DWORD)dataSize, param->filename, &url, updateProgress))) {
	case ERROR_INVALID_DATA:
		MessageBoxA(NULL, url, "Error", MB_ICONERROR);
		break;
	case 0:
		SetWindowTextA(outputEdit, url);
		if (SendMessageA(autocopyCheckbox, BM_GETCHECK, 0, 0)) {
			copyLinkToClipboard(param->parent);
		}
		break;
	default:
		errorMsgbox(errCode);
	}
    SendMessageA(progressBar, PBM_SETPOS, 0, 0);
rip:
	GlobalUnlock(param->data);
	GlobalFree(param->data);
	free(param);
	if (!url) free(url);
	EnableWindow(fileButton, TRUE);
	EnableWindow(clipButton, TRUE);
	return 0;
}

/*
*  Callback for the upload from clipboard command
*/
void uploadFromClipboard(HWND parent) {
	if (uploadThread != NULL) {
		if (WaitForSingleObject(uploadThread, INFINITE)) {
			MessageBoxA(NULL, "Failed to join the thread", "Error", MB_ICONERROR);
			return;
		}
		CloseHandle(uploadThread);
		uploadThread = NULL;
	}

	uploadParam *param = malloc(sizeof(uploadParam));
	if (param == NULL) return;
	*param->filename = 0;
	param->parent = parent;
	param->data = 0;
	switch (getPngFromClipboard(&param->data)) {
	case 1:
		MessageBoxA(NULL, "Failed to initialize GDI+", "Error", MB_ICONERROR); goto rip;
	case 2:
		MessageBoxA(NULL, "Failed to initialize the PNG encoder", "Error", MB_ICONERROR); goto rip;
	case 3:
		MessageBoxA(NULL, "Failed to open clipboard", "Error", MB_ICONERROR); goto rip;
	case 4:
		MessageBoxA(NULL, "No image in clipboard", "Error", MB_ICONWARNING); goto rip;
	case 5: case 6:
		MessageBoxA(NULL, "Failed to allocate the in-memory stream", "Error", MB_ICONERROR); goto rip;
	}

	uploadThread = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type) uploadHelper, param, 0, NULL);
	if (uploadThread == NULL) {
		MessageBoxA(NULL, "Failed to create the thread", "Error", MB_ICONERROR);
		goto rip;
	}
	return;
rip:
	if (param->data) GlobalFree(param->data);
	free(param);
}

/*
*  Callback for the upload from file command
*/
void uploadFromFile(HWND parent) {
	if (FAILED(CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY))) return;
	uploadParam* param;
	HGLOBAL buf = 0;
	HANDLE file = 0;
	if (uploadThread != NULL) {
		if (WaitForSingleObject(uploadThread, INFINITE)) {
			MessageBoxA(NULL, "Failed to join the thread", "Error", MB_ICONERROR);
			return;
		}
		CloseHandle(uploadThread);
		uploadThread = NULL;
	}

	char filepath[512];
	param = malloc(sizeof(uploadParam));
	if (!param) goto rip;
	*filepath = 0;
	*param->filename = 0;
	param->parent = parent;

	OPENFILENAMEA ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = sizeof(filepath);
	ofn.lpstrFileTitle = param->filename;
	ofn.nMaxFileTitle = sizeof(param->filename);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;

	if (!GetOpenFileNameA(&ofn)) {
		DWORD error = CommDlgExtendedError();
		if (error) errorMsgbox(error);
		goto rip;
	}
	CoUninitialize();

	file = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		MessageBoxA(NULL, "Failed to open the file", "Error", MB_ICONERROR);
		goto rip;
	}
	LARGE_INTEGER filesize;
	DWORD bytesRead;
	if (!GetFileSizeEx(file, &filesize)) {
		MessageBoxA(NULL, "Could not get file size", "Error", MB_ICONERROR);
		goto rip;
	}
	buf = GlobalAlloc(0, (size_t)filesize.QuadPart);
	if (!ReadFile(file, buf, (DWORD)filesize.QuadPart, &bytesRead, NULL)) {
		MessageBoxA(NULL, "Could not read the file", "Error", MB_ICONERROR);
		goto rip;
	}
	CloseHandle(file);
	file = 0;
	if (bytesRead != filesize.QuadPart) {
		MessageBoxA(NULL, "Could not read the entire file", "Error", MB_ICONERROR);
		goto rip;
	}
	param->data = buf;

	uploadThread = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)uploadHelper, param, 0, NULL);
	if (uploadThread == NULL) {
		MessageBoxA(NULL, "Failed to create the thread", "Error", MB_ICONERROR);
		goto rip;
	}
	return;
rip:
	if (param) free(param);
	if (file) CloseHandle(file);
	if (buf) GlobalFree(buf);
	CoUninitialize();
}

// ------------------------------------------------------------------------- //

LRESULT wndColorHelper(WPARAM wParam, COLORREF bkColor, COLORREF textColor, HBRUSH brush) {
	SetBkColor((HDC)wParam, bkColor);
	SetTextColor((HDC)wParam, textColor);
	return (LRESULT)brush;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_CREATE: {
		createControls(hWnd);
		if (initSettings()) {
			MessageBoxA(hWnd, "Could not initialize config", "Error", MB_ICONERROR);
			ExitProcess(1);
		}
		char *buf;
		DWORD errCode;
		if ((errCode = loadSettings(&buf))) {
			if (errCode != ERROR_FILE_NOT_FOUND) {
				errorMsgbox(errCode);
			}
			break;
		}
		else {
			SendMessageA(autocopyCheckbox, BM_SETCHECK, (*buf) ? BST_CHECKED : BST_UNCHECKED, 0);
			SetWindowTextA(webhookEdit, buf + 1);
		}
		break;
	}
	case WM_CTLCOLORBTN:
		return (LRESULT)bgBrush;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		switch (GetDlgCtrlID((HWND)lParam)) {
		case CT_EDIT:
			return wndColorHelper(wParam, DCOLOR_BG_BRIGHT, RGB(240, 240, 240), brightBrush);
		case CT_CAPTION:
			return wndColorHelper(wParam, DCOLOR_BG_DARK, RGB(150, 150, 150), bgBrush);
		default:
			return wndColorHelper(wParam, DCOLOR_BG_DARK, RGB(240, 240, 240), bgBrush);
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case CT_UPCLIP:
			uploadFromClipboard(hWnd);
			break;
		case CT_UPFILE:
			uploadFromFile(hWnd);
			break;
		case CT_COPY:
			copyLinkToClipboard(hWnd);
			break;
		case CT_WEBHOOK:
			if (isInSettings) {
				char webhookText[256];
				if (GetWindowTextA(webhookEdit, webhookText, sizeof webhookText) == 0) {
					MessageBoxA(hWnd, "Please enter a webhook.", "Error", MB_ICONINFORMATION);
					break;
				}
				BYTE opts = 0;
				DWORD status;
				UINT autocopy = (UINT)SendMessageA(autocopyCheckbox, BM_GETCHECK, 0, 0);
				if (autocopy) opts |= 1;
				if ((status = storeSettings(opts, webhookText))) {
					errorMsgbox(status);
				}
				isInSettings = FALSE;
				SetWindowTextA(webhookButton, "Edit");
			}
			else {
				isInSettings = TRUE;
				SetWindowTextA(webhookButton, "Save");
			}
			EnableWindow(autocopyCheckbox, isInSettings);
			EnableWindow(webhookEdit, isInSettings);
			EnableWindow(clipButton, !isInSettings);
			EnableWindow(fileButton, !isInSettings);
			break;
		}
		break;
	case WM_NCCREATE:
		return TRUE;
    default: break; // SILENCE, CLANG-TIDY!
	}
	return DefWindowProcA(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
) {
	initializeGdiObjects();
	hInst = hInstance;
	MSG msg;
	WNDCLASSEXA wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(1));
	wc.hInstance = hInstance;
	wc.hbrBackground = bgBrush;
	wc.lpszClassName = "WinDimgup";
	if (!RegisterClassExA(&wc)) return 1;

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&icc);

	HWND window = CreateWindowExA(WS_EX_CONTROLPARENT, wc.lpszClassName, "Loading...",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_BORDER, 0, 0,
		WIN_WIDTH, WIN_HEIGHT, 0, 0, hInstance, 0);

	if (!window) return 2;

	// center the window
	int xPos = (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2;
	int yPos = (GetSystemMetrics(SM_CYSCREEN) - WIN_HEIGHT) / 2;
	SetWindowPos(window, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

	SetWindowTextA(window, "WinDimgup");

	while (GetMessageA(&msg, 0, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	return 0;
}
