#pragma once
#include <Windows.h>
#include <functional>
#include <type_traits>
#include <memory>
#include <atlstr.h>

using std::enable_if_t;
using std::is_base_of;
using std::decay_t;
using std::enable_if;

class CWindowsThreadPool
{
public:
	static CWindowsThreadPool &GetDefaultThreadPool()
	{
		static CWindowsThreadPool pool;
		return pool;
	}

	CWindowsThreadPool(DWORD cthrdMin, DWORD cthrdMax, BOOL bRunLong = FALSE);
	void SubmitThreadpoolWork(const std::function<void()> &callback);
	void SubmitThreadpoolWait(HANDLE handle, FILETIME fileTime, const std::function<void()> &callback);
	BOOL SetThreadpoolSerialTimerEx(const std::function<void()> &callback, PFILETIME  ftDueTime, DWORD msWindowLength, DWORD nPeriod);
	void SubmitThreadpoolIO(const std::function<void()> &callback);

	~CWindowsThreadPool();
private:
	CWindowsThreadPool();
	static VOID NTAPI PTP_WORK_CALLBACK_impl(
		_Inout_     PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID                 Context,
		_Inout_     PTP_WORK              Work
		);

	static VOID NTAPI PTP_SERIALTIMER_CALLBACK_impl(
		_Inout_     PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID                 Context,
		_Inout_     PTP_TIMER             Timer
		);
private:
	struct CThreadLoopTimerItem;

	struct CThreadLoopItem
	{
		template<typename T, typename enable_if_t<is_base_of<CThreadLoopItem, decay_t<T>>::value>* =nullptr>
		static T* MakeItem(const std::function<void()> &callback, DWORD nLastPeriod = 1)
		{
			T *item = new T;
			item->m_callback = callback;
			item->m_nLastPeriod = nLastPeriod;

			return item;
		}

		virtual DWORD Release(){ return --m_nLastPeriod;}
		virtual void Destroy() { delete this; }
		virtual void CallBack() { m_callback(); }
		virtual ~CThreadLoopItem() { }
	private:
		volatile   DWORD		m_nLastPeriod;
		std::function<void()>	m_callback;
	};

	struct CThreadLoopTimerItem : public CThreadLoopItem
	{
	public:
		FILETIME GetFtDueTime()const { return m_ftDueTime; }
		DWORD GetMsWindowLength()const { return m_msWindowLength; }

		void SetFtDueTime(PFILETIME pftDueTime) { m_ftDueTime = *pftDueTime; }
		void SetMsWindowLength(DWORD msWindowLength){ m_msWindowLength = msWindowLength; }
	private:
		FILETIME m_ftDueTime;
		DWORD	 m_msWindowLength;
	};

	DWORD						m_managerThreadId;
	TP_CALLBACK_ENVIRON			m_cbe;
	PTP_POOL					m_ptpPool;
};