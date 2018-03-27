#pragma once

#include <Windows.h>
#include <Windowsx.h>
#include <tchar.h>
#include <functional>
#include <atlstr.h>
#include <unordered_map>

using std::unordered_map;
using std::function;

class CApp;

namespace _detail
{
	struct CThreadState
	{
		void *pThis;
		HHOOK hOldHook;
	};

	class CThreadStateModule
	{
	public:
		static CThreadStateModule &Instance()
		{
			static CThreadStateModule instance;
			return instance;
		}

		CThreadState &GetData();

		~CThreadStateModule() { ::DeleteCriticalSection(&m_crtSection); }
	private:
		CThreadStateModule(){ ::InitializeCriticalSection(&m_crtSection); }

		CThreadStateModule(const CThreadStateModule &rhs) = delete;
		CThreadStateModule &operator=(const CThreadStateModule &rhs) = delete;
	private:
		std::unordered_map<DWORD, CThreadState> m_thread2app;
		::CRITICAL_SECTION						m_crtSection;
	};
}


/*
 * ·â×°windowProc
 **/
class CApp
{
public:
	CApp() = default;
	BOOL Create(HINSTANCE hInstance, LPTSTR wndName);
	virtual ~CApp();

	static CApp *FromHandlePermanent(HWND hwnd);
	static void Attach(HWND hwnd, CApp *);
	HWND GetHwnd()const { return m_hwnd; }

	UINT_PTR Run();
	BOOL PostMessage(_In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam);
	BOOL RegistMessageHandler(UINT msg, const function<LRESULT(HWND, UINT, WPARAM, LPARAM)> &func, bool bForce = false);
private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK CreateWndHook(int code, WPARAM wParam, LPARAM lParam);

private:
	CString					m_wndName;

	HWND					m_hwnd;
	WNDCLASS				m_wndclass;
	HINSTANCE				m_hInstance;

	unordered_map<UINT, function<LRESULT(HWND, UINT, WPARAM, LPARAM)>> m_mapMsg;
};