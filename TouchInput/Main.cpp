#include <Windows.h>
//#include <ShellScalingAPI.h>
#include <stdio.h>
#include <fstream>
#include <map>

//#pragma comment(lib, "shcore.lib")

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HBITMAP hBitmap = NULL;
std::map<int, POINT> pressed;
wchar_t *configFileName;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	configFileName = new wchar_t[MAX_PATH];
	MultiByteToWideChar(CP_ACP, NULL, lpCmdLine, -1, configFileName, MAX_PATH);
	WNDCLASSEX WindowClass = { 0 };
	HWND hWnd;
	MSG msg;
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = hInstance;
	WindowClass.lpszClassName = (LPCTSTR)L"qwe";
	RegisterClassEx(&WindowClass);
	hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE, WindowClass.lpszClassName, L"asd", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, //WS_TILEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
	ShowWindow(hWnd, nCmdShow);
	if (!RegisterTouchWindow(hWnd, 0))
	{
		MessageBox(hWnd, L"Cannot register application window for multitouch input", L"Error", MB_OK);
		return FALSE;
	}

	while (GetMessage(&msg, 0, 0, 0) == TRUE)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}

struct button
{
	int left;
	int top;
	int right;
	int bottom;
	int key;
	bool pressed;
};

int buttonsCount;
button *buttons;


POINT getRelativeLocation(HWND hWnd, int x, int y)
{
	RECT wa;
	RECT ca;
	GetClientRect(hWnd, &ca);
	GetWindowRect(hWnd, &wa);
	int border = (wa.right - wa.left - ca.right + ca.left) / 2;
	int caption = GetSystemMetrics(SM_CYSMCAPTION);
	int relx = (x / 100 - wa.left - border) * 1000 / ca.right;
	int rely = (y / 100 - wa.top - caption - border) * 1000 / ca.bottom;
	POINT loc{ x = relx, y = rely };
	return loc;
}

bool atButton(POINT point, button btn) {
	return point.x > btn.left && point.x < btn.right && point.y > btn.top && point.y < btn.bottom;
}

void sendKey(int key, bool up = false)
{
	INPUT inp = { 0 };
	KEYBDINPUT ki = { 0 };
	if (up)
		ki.dwFlags = KEYEVENTF_KEYUP;

	ki.wVk = key;
	inp.ki = ki;
	inp.type = INPUT_KEYBOARD;
	SendInput(1, &inp, sizeof(INPUT));
}

void DecodeTouch(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	unsigned int inputCount = (unsigned int)wParam;
	TOUCHINPUT * ti = new TOUCHINPUT[inputCount];
	BOOL r = GetTouchInputInfo((HTOUCHINPUT)lParam, inputCount, ti, sizeof(TOUCHINPUT));

	if (r)
	{
		for (unsigned int i = 0; i < inputCount; ++i)
		{
			if (ti[i].dwFlags & TOUCHEVENTF_DOWN)
			{
				POINT pos = getRelativeLocation(hWnd, ti[i].x, ti[i].y);
				pressed[ti[i].dwID] = pos;
				for (int j = 0; j < buttonsCount; ++j)
					if (atButton(pos, buttons[j]))
						sendKey(buttons[j].key);
			}
			else if (ti[i].dwFlags & TOUCHEVENTF_MOVE)
			{
				auto lastPos = pressed.find(ti[i].dwID);
				POINT pos = getRelativeLocation(hWnd, ti[i].x, ti[i].y);
				if (lastPos != pressed.end())
					for (int j = 0; j < buttonsCount; ++j)
					{
						if (atButton(lastPos->second, buttons[j]) && !atButton(pos, buttons[j]))
							sendKey(buttons[j].key, true);
						else if (!atButton(lastPos->second, buttons[j]) && atButton(pos, buttons[j]))
							sendKey(buttons[j].key);
					}
				pressed[ti[i].dwID] = pos;

			}
			else if (ti[i].dwFlags & TOUCHEVENTF_UP)
			{
				auto lastPos = pressed.find(ti[i].dwID);
				if (lastPos != pressed.end())
				{
					for (int j = 0; j < buttonsCount; ++j)
						if (atButton(lastPos->second, buttons[j]))
							sendKey(buttons[j].key, true);

					pressed.erase(lastPos);
				}
			}

		}

		CloseTouchInputHandle((HTOUCHINPUT)lParam);
	}

	delete[] ti;
}



void init(HWND hWnd)
{
	SetProcessDPIAware();
	if (configFileName[0] == 0)
		wcscpy(configFileName, L"default.txt");
	wchar_t *bgName = new wchar_t[100];
	std::wifstream in(configFileName);
	delete[] configFileName;
	int w, h;
	in >> bgName;
	in >> w;
	in >> h;
	in >> buttonsCount;
	buttons = new button[buttonsCount];
	int i = 0;
	while (i < buttonsCount && !in.eof())
	{
		button b{ 0 };
		int code;
		wchar_t c;
		in >> b.left;
		in >> b.top;
		in >> b.right;
		in >> b.bottom;
		in >> code;
		b.key = code;
		if (code == 0)
		{
			in >> c;
			b.key = c;
		}

		buttons[i] = b;
		++i;
	}

	in.close();

	hBitmap = (HBITMAP)LoadImage(NULL, bgName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	delete[] bgName;

	RECT wa;
	RECT ca;
	GetClientRect(hWnd, &ca);
	GetWindowRect(hWnd, &wa);
	int border = abs(wa.right - wa.left - ca.right + ca.left) / 2;
	int caption = GetSystemMetrics(SM_CYSMCAPTION);
	
	w += (border + border);
	h += (border + caption);

	int sw = GetSystemMetrics(SM_CYSCREEN);
	int sh = GetSystemMetrics(SM_CYSCREEN);
	SetWindowPos(hWnd, NULL, 2, sh - h, w, h, NULL);
	BOOL bFalse = FALSE;
	SetWindowFeedbackSetting(hWnd, FEEDBACK_TOUCH_TAP, NULL, sizeof(BOOL), &bFalse);
	SetWindowFeedbackSetting(hWnd, FEEDBACK_TOUCH_RIGHTTAP, NULL, sizeof(BOOL), &bFalse);
	SetWindowFeedbackSetting(hWnd, FEEDBACK_TOUCH_PRESSANDHOLD, NULL, sizeof(BOOL), &bFalse);
}

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_TOUCH:
		DecodeTouch(hWnd, wParam, lParam);
		break;
	case WM_CREATE:
		init(hWnd);
		break;
	case WM_PAINT:
		PAINTSTRUCT     ps;
		HDC             hdc;
		BITMAP          bitmap;
		HDC             hdcMem;
		HGDIOBJ         oldBitmap;

		hdc = BeginPaint(hWnd, &ps);

		hdcMem = CreateCompatibleDC(hdc);
		oldBitmap = SelectObject(hdcMem, hBitmap);

		GetObject(hBitmap, sizeof(bitmap), &bitmap);

		RECT ca;
		GetClientRect(hWnd, &ca);
		StretchBlt(hdc, 0, 0, ca.right, ca.bottom, hdcMem, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
		//BitBlt(hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);

		SelectObject(hdcMem, oldBitmap);
		DeleteDC(hdcMem);

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		delete[] buttons;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}