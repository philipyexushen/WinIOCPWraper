#include "Universal.h"
#include <memory>
#include <signal.h>
#include <assert.h>

using namespace std;

namespace _detail
{
	CThreadState &CThreadStateModule::GetData()
	{
		::EnterCriticalSection(&m_crtSection);
		CThreadState &state = m_thread2app[GetCurrentThreadId()];
		::LeaveCriticalSection(&m_crtSection);
		return state;
	}
}

namespace
{
	PSRWLOCK g_hwndAppMapSrwLock()
	{
		static RTL_SRWLOCK lock;
		static PSRWLOCK plock;
		if (plock == nullptr)
		{
			//不用Delete了，程序结束也会收回的
			InitializeSRWLock(&lock);
			plock = &lock;
		}
		return plock;
	}

	unordered_map<HWND, CApp *>	g_mapHwndApp;
}

CApp::~CApp()
{

}

CApp *CApp::FromHandlePermanent(HWND hwnd)
{
	CApp *app = nullptr;
	::AcquireSRWLockShared(g_hwndAppMapSrwLock());

	auto &&iter = g_mapHwndApp.find(hwnd);
	ATLASSERT(iter != g_mapHwndApp.end() && iter->second != nullptr);

	if (iter != g_mapHwndApp.end())
		app = iter->second;

	ATLASSERT(app != nullptr);
	::ReleaseSRWLockShared(g_hwndAppMapSrwLock());

	return app;
}

void CApp::Attach(HWND hwnd, CApp *app)
{
	::AcquireSRWLockExclusive(g_hwndAppMapSrwLock());
	ATLASSERT(g_mapHwndApp.find(hwnd) == g_mapHwndApp.end());
	g_mapHwndApp[hwnd] = app;
	::ReleaseSRWLockExclusive(g_hwndAppMapSrwLock());
}

BOOL CApp::Create(HINSTANCE hInstance, LPTSTR wndName)
{
	m_hInstance = hInstance;
	m_wndclass.style = CS_HREDRAW | CS_VREDRAW;
	m_wndclass.lpfnWndProc = CApp::WindowProc;
	m_wndclass.cbClsExtra = 0;
	m_wndclass.cbWndExtra = 0;
	m_wndclass.hInstance = m_hInstance;
	m_wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	m_wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	m_wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	m_wndclass.lpszMenuName = NULL;
	m_wndclass.lpszClassName = wndName;

	m_wndName = wndName;
	if (!RegisterClass(&m_wndclass))
	{
		MessageBox(NULL, TEXT("This program requires Windows NT!"),
			m_wndName, MB_ICONERROR);
		return FALSE;
	}

	auto &appHwndModule = ::_detail::CThreadStateModule::Instance();
	auto &threadState = appHwndModule.GetData();
	threadState.hOldHook = ::SetWindowsHookEx(WH_CBT, CApp::CreateWndHook, nullptr, GetCurrentThreadId());
	threadState.pThis = this;

	m_hwnd = CreateWindow(m_wndName,
		m_wndName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, m_hInstance, nullptr);

	return TRUE;
}

UINT_PTR CApp::Run()
{
	MSG  msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<INT>(msg.wParam);
}

LRESULT CALLBACK CApp::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto &&app = CApp::FromHandlePermanent(hwnd);

	LRESULT ret;

	auto &&iter = app->m_mapMsg.find(msg);
	if (iter != app->m_mapMsg.end())
	{
		ret = iter->second(hwnd, msg, wParam, lParam);
		return ret;
	}

	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CApp::CreateWndHook(int code, WPARAM wParam, LPARAM lParam)
{
	auto &&threadStateModule = ::_detail::CThreadStateModule::Instance();
	auto &&threadState = threadStateModule.GetData();

	auto &&hOldHook = threadState.hOldHook;
	ATLASSERT(hOldHook != nullptr);
	
	if (code != HCBT_CREATEWND)
	{
		return ::CallNextHookEx(hOldHook, code, wParam, lParam);
	}

	HWND hwnd = (HWND)wParam;
	LPCREATESTRUCT lpcs = ((LPCBT_CREATEWND)lParam)->lpcs;

	CApp *app = reinterpret_cast<CApp *>(threadState.pThis);

	ATLASSERT(app != nullptr);
	ATLASSERT(hwnd != nullptr);
	
	app->m_hwnd = hwnd;
	CApp::Attach(hwnd, app);

	return ::CallNextHookEx(hOldHook, code, wParam, lParam);
}

BOOL CApp::RegistMessageHandler(UINT msg, const function<LRESULT(HWND, UINT, WPARAM, LPARAM)> &func, bool bForce)
{
	if (bForce)
	{
		m_mapMsg[msg] = func;
		return TRUE;
	}
	else
	{
		auto iter = m_mapMsg.find(msg);
		if (iter == m_mapMsg.end())
		{
			m_mapMsg.insert({ msg, func });
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CApp::PostMessage(_In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return ::PostMessage(m_hwnd, Msg, wParam, lParam);
}