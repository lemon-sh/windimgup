#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>

// ------------------------------------------------------------------------- //

// handles
HINSTANCE hInst;
HFONT uiFont, titleFont;
HBRUSH bgBrush;

// window size
#define WIN_WIDTH 300
#define WIN_HEIGHT 200

// colors
#define DCOLOR_BG RGB( 40, 40, 40)

// controls
enum ControlClass {
	CT_TEXT, CT_CHECK, CT_UPCLIP, CT_WEBHOOK
};

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
	HWND checkbox = CreateWindowExA(0, "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_MULTILINE, posX, posY, width, height, parent, (HMENU)controlId, hInst, 0);
	SendMessageA(checkbox, WM_SETFONT, (WPARAM)uiFont, MAKELPARAM(TRUE, 0));
	return checkbox;
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
	createLabel(hWnd, CT_TEXT, "WinDimgup", titleFont, 10, 10, 130, 40);
	createEdit(hWnd, CT_);
	createCheckbox(hWnd, CT_CHECK, 10, 130, 20, 20);
}

/*
*  Callback for the upload from clipboard command
*/
void uploadFromClipboard() {

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
	case WM_CREATE:
		createControls(hWnd);
		break;
	case WM_CTLCOLORBTN:
		return (LRESULT)bgBrush;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		switch (GetDlgCtrlID((HWND)lParam)) {
		default:
			return wndColorHelper(wParam, DCOLOR_BG, RGB(240, 240, 240), bgBrush);
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case CT_UPCLIP:
			uploadFromClipoard();
			break;
		}
		break;
	case WM_NCCREATE:
		return TRUE;
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
	}
	return 0;
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
