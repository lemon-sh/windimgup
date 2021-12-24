#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <Commctrl.h>
#include <stdio.h>

#include "settings.h"
#include "discord_webhook.h"
#include "clipboard.h"
#include "common_errors.h"

#define METASTRING(x) x,(sizeof(x)-1)

// ------------------------------------------------------------------------- //

// handles
HINSTANCE hInst;
HFONT uiFont, titleFont;
HBRUSH bgBrush;
HANDLE uploadThread;

// window size
#define WIN_WIDTH 300
#define WIN_HEIGHT 220

// colors
#define DCOLOR_BG RGB( 40, 40, 40)

// controls
enum ControlClass {
	CT_TEXT, CT_CHECK, CT_UPCLIP, CT_WEBHOOK, CT_UPFILE, CT_PROGRESS, CT_COPY
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
	HWND progressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, posX, posY, width, height, parent, (HMENU)controlId, hInst, NULL);
	SendMessageA(progressBar, PBM_SETRANGE, 0, (LPARAM)MAKELONG(0, max));
	return progressBar;
}

// ------------------------------------------------------------------------- //

/*
*  Initialize fonts and brushes
*/
void initializeGdiObjects() {
	uiFont = CreateFontA(16, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, FF_DONTCARE, "Segoe UI");
	titleFont = CreateFontA(32, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, FF_DONTCARE, "Segoe UI");
	bgBrush = CreateSolidBrush(DCOLOR_BG);
}

/*
*  Creates the UI
*/
void createControls(HWND hWnd) {
	createLabel(hWnd, CT_TEXT, "WinDimgup", titleFont, 10, 8, 130, 40);
	createLabel(hWnd, CT_TEXT, "Upload:", uiFont, 12, 50, 50, 22);
	fileButton = createButton(hWnd, CT_UPFILE, "File", 60, 47, 60, 22);
	clipButton = createButton(hWnd, CT_UPCLIP, "Image from clipboard", 120, 47, 150, 22);
	progressLabel = createLabel(hWnd, CT_TEXT, "", uiFont, 12, 70, 50, 22);
	progressBar = createProgress(hWnd, CT_PROGRESS, 100, 61, 70, 208, 18);
	outputEdit = createEdit(hWnd, CT_TEXT, 12, 95, 200, 22, ES_READONLY | ES_AUTOHSCROLL);
	createButton(hWnd, CT_COPY, "Copy", 215, 95, 55, 22);

	webhookEdit = createEdit(hWnd, CT_TEXT, 12, 125, 200, 22, ES_AUTOHSCROLL);
	EnableWindow(webhookEdit, 0);
	webhookButton = createButton(hWnd, CT_WEBHOOK, "Edit", 215, 125, 55, 22);
	autocopyCheckbox = createCheckbox(hWnd, CT_CHECK, 12, 150, 15, 20);
	createLabel(hWnd, CT_TEXT, "Automatically copy link to clipboard", uiFont, 30, 152, 200, 20);
	EnableWindow(autocopyCheckbox, 0);

	SendMessageA(webhookEdit, EM_SETCUEBANNER, 1, (LPARAM)L"Webhook...");
}

/*
*  Callback for the uploadWebhook function
*/
void updateProgress(DWORD a, DWORD b) {
	int percent = a*100 / b;
	char progressText[8];
	snprintf(progressText, sizeof progressText, "%d%%", percent);
	SendMessageA(progressBar, PBM_SETPOS, percent, 0);
	SetWindowTextA(progressLabel, progressText);
}

/*
*  Display error box from an error code
*/
void errorMsgbox(DWORD errorCode)
{
	LPSTR lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
		GetModuleHandleA("wininet.dll"), errorCode, 0, (LPSTR) & lpMsgBuf, 0, NULL);
	MessageBoxA(NULL, lpMsgBuf, "Error", MB_ICONERROR);
	LocalFree(lpMsgBuf);
}

typedef struct {
	HGLOBAL data;
	char* filename;
	BOOL filenameAlloc;
} uploadParam;

/*
*  Handles upload in a window context. Should be run in a thread.
*/
DWORD uploadHelper(uploadParam *param) {
	char webhook[256];
	char *webhook_start = webhook, *dataLocked = GlobalLock(param->data), *url = 0;
	SIZE_T dataSize = GlobalSize(param->data);
	DWORD errCode = 0;

	if (dataSize == 0) goto brazil;
	if (dataLocked == NULL) goto brazil;
	EnableWindow(fileButton, FALSE);
	EnableWindow(clipButton, FALSE);
	if (!GetWindowTextA(webhookEdit, webhook, sizeof(webhook))) {
		MessageBoxA(NULL, "Empty webhook", "Error", MB_ICONERROR);
		goto brazil;
	}
	if (!strncmp(webhook, METASTRING("https://discord.com"))) {
		webhook_start += sizeof("https://discord.com") - 1;
	}

	switch ((errCode = uploadWebhook(webhook_start, dataLocked, (DWORD)dataSize, param->filename, &url, updateProgress, param->filenameAlloc))) {
	case errUnknown:
		MessageBoxA(NULL, url, "Error", MB_ICONERROR);
		goto brazil;
	case 0:
		SetWindowTextA(outputEdit, url);
		break;
	default:
		errorMsgbox(errCode);
	}
brazil:
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
void uploadFromClipboard() {
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
	param->filename = "wdmupload.png";
	param->filenameAlloc = FALSE;
	switch (getPngFromClipboard(&param->data)) {
	case 1:
		MessageBoxA(NULL, "Failed to initialize GDI+", "Error", MB_ICONERROR); return;
	case 2:
		MessageBoxA(NULL, "Failed to initialize the PNG encoder", "Error", MB_ICONERROR); return;
	case 3:
		MessageBoxA(NULL, "Failed to open clipboard", "Error", MB_ICONERROR); return;
	case 4:
		MessageBoxA(NULL, "No image in clipboard", "Error", MB_ICONWARNING); return;
	case 5: case 6:
		MessageBoxA(NULL, "Failed to allocate the in-memory stream", "Error", MB_ICONERROR); return;
	}

	uploadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) uploadHelper, param, 0, NULL);
	if (uploadThread == NULL) {
		MessageBoxA(NULL, "Failed to create the thread", "Error", MB_ICONERROR);
		GlobalFree(param->data);
		free(param);
	}
}

/*
*  Callback for the upload from file command
*/
void uploadFromFile() {
	uploadParam* param = 0;
	HGLOBAL buf = 0;
	if (uploadThread != NULL) {
		if (WaitForSingleObject(uploadThread, INFINITE)) {
			MessageBoxA(NULL, "Failed to join the thread", "Error", MB_ICONERROR);
			return;
		}
		CloseHandle(uploadThread);
		uploadThread = NULL;
	}

	if (FAILED(CoInitializeEx(NULL, 0))) errorMsgbox(GetLastError());
	char filename[MAX_PATH] = {0};
	OPENFILENAMEA ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFile = filename;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

	if (!GetOpenFileNameA(&ofn)) errorMsgbox(CommDlgExtendedError());
	CoUninitialize();

	HANDLE file = CreateFileA("a.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		MessageBoxA(NULL, "Failed to open the file", "Error", MB_ICONERROR);
		return;
	}
	LARGE_INTEGER filesize;
	DWORD bytesRead;
	if (!GetFileSizeEx(file, &filesize)) {
		MessageBoxA(NULL, "Could not get file size", "Error", MB_ICONERROR);
		goto rip;
	}
	buf = GlobalAlloc(0, filesize.QuadPart);
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
	param = malloc(sizeof(uploadParam));
	if (param == NULL) goto rip;
	param->data = buf;
	param->filename = _strdup("a.txt");
	param->filenameAlloc = TRUE;

	uploadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)uploadHelper, param, 0, NULL);
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
			SendMessageA(autocopyCheckbox, BM_SETCHECK, *buf ? BST_CHECKED : BST_UNCHECKED, 0);
			SetWindowTextA(webhookEdit, buf + 1);
		}
		break;
	}
	case WM_CTLCOLORBTN:
		return (LRESULT)bgBrush;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		return wndColorHelper(wParam, DCOLOR_BG, RGB(240, 240, 240), bgBrush);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case CT_UPCLIP:
			uploadFromClipboard();
			break;
		case CT_UPFILE:
			uploadFromFile();
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
	WNDCLASSA wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = bgBrush;
	wc.lpszClassName = "WinDimgup";
	if (!RegisterClassA(&wc)) return 1;

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
