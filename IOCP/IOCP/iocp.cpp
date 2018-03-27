#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <process.h>
#include <iostream>
#include <atlbase.h>
#include <atlwin.h>
#include <atlcom.h>
#include "Universal.h"
#include "ThreadLoop.h"

using namespace std;

#define WM_USER_FUCK WM_USER + 1

volatile LONG64 g_nCurrentWork;

INT WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nShowCmd)
{
	CApp app;
	app.Create(hInstance, L"Fuck");

	app.RegistMessageHandler(
		WM_GETMINMAXINFO, [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		OutputDebugString(L"Hello Wolrd\n");
		return ::DefWindowProc(hwnd, msg, wParam, lParam);
	});

	app.RegistMessageHandler(
		WM_USER_FUCK, [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		CString str;
		str.Format(L"Worker %d has sended a message\n", lParam);
		OutputDebugString(str);
		return ::DefWindowProc(hwnd, msg, wParam, lParam);
	});

	CWindowsThreadPool &dtp = CWindowsThreadPool::GetDefaultThreadPool();
	dtp.SubmitThreadpoolWork(
		[]()
	{
		Sleep(3000);
		LONG64 number = ::InterlockedIncrement64(&g_nCurrentWork);
		CString str;
		str.Format(L"I am %lld\n", number);
		OutputDebugString(str);
	});

	HWND appHwnd = app.GetHwnd();
	LARGE_INTEGER li;
	li.QuadPart = -5000000;
	FILETIME ft;
	ft.dwHighDateTime = li.HighPart;
	ft.dwLowDateTime = li.LowPart;

	dtp.SetThreadpoolSerialTimerEx(
		[appHwnd]()
	{
		LONG64 number = ::InterlockedIncrement64(&g_nCurrentWork);
		::PostMessage(appHwnd, WM_USER_FUCK, 0, number);
	}, &ft, 0, 5);

	CString strName(L"Œ“ «defphilip");

	dtp.SubmitThreadpoolWork(
		[strName]()
	{
		Sleep(2000);
		LONG64 number = ::InterlockedIncrement64(&g_nCurrentWork);
		
		CString str;
		str.Format(L"%s %lld\n", strName, number);
		OutputDebugString(str);
	});

	return app.Run();
}