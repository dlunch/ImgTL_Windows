#include <windows.h>
#include <CommCtrl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")

#include <exception>
#include <string>

#include "resource.h"
#include "Uploader.h"
#include "Grabber.h"

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::string userEmail;
Uploader uploader;
Grabber grabber;
HWND mainWnd;
DWORD fullScreenHotKey;
DWORD activeWndHotKey;
DWORD regionHotKey;

void drawRegionLine(int sx, int sy, int ex, int ey)
{
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

	HDC hdc = CreateCompatibleDC(GetDC(NULL));
	HBITMAP hbmp = CreateCompatibleBitmap(GetDC(NULL), width, height);
	HBITMAP oldbmp = reinterpret_cast<HBITMAP>(SelectObject(hdc, hbmp));
	{
		Gdiplus::SolidBrush transparent(Gdiplus::Color(1, 0, 0, 0));
		Gdiplus::Graphics g(hdc);
		Gdiplus::Pen line(Gdiplus::Color(255, 255, 0, 0));

		g.FillRectangle(&transparent, left, top, width, height);
		if(ex < sx)
			std::swap(ex, sx);
		if(ey < sy)
			std::swap(ey, sy);
		g.DrawRectangle(&line, sx, sy, ex - sx, ey - sy);

		g.Flush();
	}

	BLENDFUNCTION bf;
	bf.AlphaFormat = AC_SRC_ALPHA;
	bf.BlendFlags = 0;
	bf.BlendOp = AC_SRC_OVER;
	bf.SourceConstantAlpha = 255;

	POINT pt = {0,};
	SIZE size = {width, height};
	UpdateLayeredWindow(mainWnd, GetDC(NULL), &pt, &size, hdc, &pt, RGB(0, 0, 0), &bf, ULW_ALPHA);
	
	DeleteObject(SelectObject(hdc, oldbmp));
	DeleteDC(hdc);
}

void startRegionGrab()
{
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

	ShowWindow(mainWnd, SW_SHOW);
	MoveWindow(mainWnd, left, top, width, height, TRUE);
	drawRegionLine(0, 0, 0, 0);
}

void endRegionGrab()
{
	ShowWindow(mainWnd, SW_HIDE);
}

void saveToClipboard(const std::string &url)
{
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, url.size());
	memcpy(GlobalLock(hMem), url.c_str(), url.size());
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
}

void processRegionGrab(int sx, int sy, int ex, int ey)
{
	std::vector<uint8_t> image(grabber.grab(sx, sy, ex, ey));
	std::string url = uploader.upload(image);
	saveToClipboard(url);
}

void processHotKey(int type)
{
	if(type == 0)
	{
		std::vector<uint8_t> image(grabber.grab());
		std::string url = uploader.upload(image);
		saveToClipboard(url);
	}
	else if(type == 1)
		startRegionGrab();
	else if(type == 2)
	{
		std::vector<uint8_t> image(grabber.grabActive());
		std::string url = uploader.upload(image);
		saveToClipboard(url);
	}
}

void registerHotKey()
{
	RegisterHotKey(mainWnd, 0, HIBYTE(LOWORD(fullScreenHotKey)), LOBYTE(LOWORD(fullScreenHotKey)));
	RegisterHotKey(mainWnd, 1, HIBYTE(LOWORD(regionHotKey)), LOBYTE(LOWORD(regionHotKey)));
	RegisterHotKey(mainWnd, 2, HIBYTE(LOWORD(activeWndHotKey)), LOBYTE(LOWORD(activeWndHotKey)));
}

void unregisterHotKey()
{
	UnregisterHotKey(mainWnd, 2);
	UnregisterHotKey(mainWnd, 1);
	UnregisterHotKey(mainWnd, 0);
}

int CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
int CALLBACK TokenDlgProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	WSADATA ws;
	WSAStartup(MAKEWORD(2, 2), &ws);

	CoInitialize(NULL);

	//We need to create windows to receive hotkey notifications

	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = TEXT("Screenshot");
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.hCursor = LoadCursor(NULL, IDC_CROSS);

	try
	{
		HKEY key;
		RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\dlunch\\imgtl"), 0, NULL, 0, KEY_ALL_ACCESS, 0, &key, NULL);
		if(!key)
			throw std::exception("Error opening registry");
		
		TCHAR temp[255];
		DWORD len = 255;
		int result;
		if((result = RegGetValue(key, nullptr, TEXT("token"), RRF_RT_REG_SZ, nullptr, temp, &len)) == 0)
			uploader.setToken(std::string(temp, temp + len / 2 - 1));

		if(result != 0 || !len)
			DialogBox(hInstance, MAKEINTRESOURCE(IDD_TOKEN), NULL, reinterpret_cast<DLGPROC>(TokenDlgProc));
		len = 4;

		if(RegGetValue(key, nullptr, TEXT("fullScreenHotKey"), RRF_RT_REG_DWORD, nullptr, &fullScreenHotKey, &len))
			fullScreenHotKey = VK_SNAPSHOT;
		if(RegGetValue(key, nullptr, TEXT("activeWndHotKey"), RRF_RT_REG_DWORD, nullptr, &activeWndHotKey, &len))
			activeWndHotKey = MAKEWORD(VK_SNAPSHOT, MOD_ALT);
		if(RegGetValue(key, nullptr, TEXT("regionHotKey"), RRF_RT_REG_DWORD, nullptr, &regionHotKey, &len))
			regionHotKey = MAKEWORD(VK_SNAPSHOT, MOD_SHIFT);

		RegCloseKey(key);
	}
	catch(...)
	{
		MessageBox(NULL, TEXT("Failed to open config"), TEXT("Error"), MB_OK);
		return 0;		
	}
	//test connection
	userEmail = uploader.getUserEmail();
	if(userEmail.size() == 0)
	{
		MessageBox(NULL, TEXT("Can't get user info"), TEXT("Error"), MB_OK);
		return 0;
	}

	mainWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
		(LPCWSTR)RegisterClassEx(&wcex), TEXT("Screenshot"), WS_POPUP, -1, -1, -1, -1, NULL, NULL, NULL, NULL);
	drawRegionLine(0, 0, 0, 0);
	ShowWindow(mainWnd, SW_HIDE);

	NOTIFYICONDATA nid = {0, };
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = mainWnd;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_APP + 1;
	nid.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_APPLICATION));
	wcscpy_s(nid.szTip, TEXT("Imgtl"));
	
	Shell_NotifyIcon(NIM_ADD, &nid);

	MSG msg;
	while(GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &nid);

	CoUninitialize();
	WSACleanup();
	Gdiplus::GdiplusShutdown(gdiplusToken);

	return msg.wParam;
}

int CALLBACK TokenDlgProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch(iMessage)
	{
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) 
			{ 
			case IDOK: 
				{
					TCHAR temp[255];
					int len = GetDlgItemText(hWnd, IDC_TOKEN, temp, 255);
					if(!len)
						return TRUE;
					std::wstring wstemp(temp, temp + len);
					uploader.setToken(std::string(temp, temp + len));
					userEmail = uploader.getUserEmail();
					if(userEmail.size() == 0)
					{
						MessageBox(NULL, TEXT("Can't get user info"), NULL, MB_OK);
						return TRUE;
					}

					HKEY key;
					RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\dlunch\\imgtl"), 0, NULL, 0, KEY_ALL_ACCESS, 0, &key, NULL);
					if(RegSetValueEx(key, TEXT("token"), 0, REG_SZ, reinterpret_cast<const BYTE *>(&wstemp[0]), wstemp.size() * 2) != 0)
						MessageBox(NULL, TEXT("Can't set registry"), NULL, MB_OK);
					RegCloseKey(key);

					EndDialog(hWnd, 0);
				}
				break;
			case IDCANCEL:
				EndDialog(hWnd, 0);
				break;
			}

			return TRUE;
		}
	}
	return FALSE;
}

int CALLBACK SettingProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch(iMessage)
	{
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(hWnd, IDC_FULL_HOTKEY), HKM_SETHOTKEY, fullScreenHotKey, 0);
		SendMessage(GetDlgItem(hWnd, IDC_ACTIVE_HOTKEY), HKM_SETHOTKEY, activeWndHotKey, 0);
		SendMessage(GetDlgItem(hWnd, IDC_REGION_HOTKEY), HKM_SETHOTKEY, regionHotKey, 0);
		SetDlgItemText(hWnd, IDC_LOGGED_USER, (std::wstring(L"로그인 된 사용자: ") + std::wstring(userEmail.begin(), userEmail.end())).c_str());
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) 
			{ 
			case IDOK: 
				{
					fullScreenHotKey = SendMessage(GetDlgItem(hWnd, IDC_FULL_HOTKEY), HKM_GETHOTKEY, 0, 0);
					activeWndHotKey = SendMessage(GetDlgItem(hWnd, IDC_ACTIVE_HOTKEY), HKM_GETHOTKEY, 0, 0);
					regionHotKey = SendMessage(GetDlgItem(hWnd, IDC_REGION_HOTKEY), HKM_GETHOTKEY, 0, 0);
					unregisterHotKey();
					registerHotKey();

					HKEY key;
					RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\dlunch\\imgtl"), 0, NULL, 0, KEY_ALL_ACCESS, 0, &key, NULL);
					RegSetValueEx(key, TEXT("fullScreenHotKey"), 0, REG_DWORD, reinterpret_cast<const BYTE *>(&fullScreenHotKey), sizeof(fullScreenHotKey));
					RegSetValueEx(key, TEXT("regionHotKey"), 0, REG_DWORD, reinterpret_cast<const BYTE *>(&regionHotKey), sizeof(regionHotKey));
					RegSetValueEx(key, TEXT("activeWndHotKey"), 0, REG_DWORD, reinterpret_cast<const BYTE *>(&activeWndHotKey), sizeof(activeWndHotKey));
					RegCloseKey(key);

					EndDialog(hWnd, 0);
				}
				break;
			case IDCANCEL:
				EndDialog(hWnd, 0);
				break;
			case IDC_CHANGE_TOKEN:
				DialogBox(GetModuleHandle(0), MAKEINTRESOURCE(IDD_TOKEN), NULL, reinterpret_cast<DLGPROC>(TokenDlgProc));
				SetDlgItemText(hWnd, IDC_LOGGED_USER, (std::wstring(L"로그인 된 사용자: ") + std::wstring(userEmail.begin(), userEmail.end())).c_str());
				break;
			}

			return TRUE;
		}
	}
	return FALSE;
}


int CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	static bool mouseDown = false;
	static POINT startPos = {0, 0};
	static POINT lastPos = {0, 0};
	switch(iMessage)
	{
	case WM_CREATE:
		mainWnd = hWnd;
		registerHotKey();
		break;
	case WM_HOTKEY:
		processHotKey(static_cast<int>(wParam));
		break;
	case WM_DESTROY:
		unregisterHotKey();
		PostQuitMessage(0);
		break;
	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		mouseDown = true;
		startPos.x = LOWORD(lParam);
		startPos.y = HIWORD(lParam);
		ClientToScreen(hWnd, &startPos);
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		mouseDown = false;

		lastPos.x = LOWORD(lParam);
		lastPos.y = HIWORD(lParam);
		ClientToScreen(hWnd, &lastPos);
		endRegionGrab();
		processRegionGrab(startPos.x, startPos.y, lastPos.x, lastPos.y);
		break;
	case WM_MOUSEMOVE:
		if(mouseDown)
		{
			lastPos.x = LOWORD(lParam);
			lastPos.y = HIWORD(lParam);
			ClientToScreen(hWnd, &lastPos);
			drawRegionLine(startPos.x, startPos.y, lastPos.x, lastPos.y);
		}
		break;
	case WM_APP + 1: //tray
		if(LOWORD(lParam) == WM_RBUTTONUP)
		{
			POINT pt;
			GetCursorPos(&pt);
			HMENU menu = GetSubMenu(LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_POPUP)), 0);
			int result = TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
			if(result == ID_POPUP_EXIT)
				PostQuitMessage(0);
			else if(result == ID_POPUP_SETTINGS)
				DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTING), NULL, reinterpret_cast<DLGPROC>(SettingProc));
		}
	}
	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}