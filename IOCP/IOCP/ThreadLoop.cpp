#include "ThreadLoop.h"
#include <threadpoolapiset.h>
#include <process.h>

#define WM_DESTROY_ITEM WM_USER + 1

CWindowsThreadPool::CWindowsThreadPool()
	: m_ptpPool(nullptr)
{
	ZeroMemory(&m_cbe, sizeof(m_cbe));
}

CWindowsThreadPool::CWindowsThreadPool(DWORD cthrdMin, DWORD cthrdMax, BOOL bRunLong)
{
	CWindowsThreadPool();
	m_ptpPool = ::CreateThreadpool(nullptr);
	::SetThreadpoolThreadMinimum(m_ptpPool, cthrdMin);
	::SetThreadpoolThreadMaximum(m_ptpPool, cthrdMax);

	::InitializeThreadpoolEnvironment(&m_cbe);

	if (bRunLong)
		::SetThreadpoolCallbackRunsLong(&m_cbe);

	::SetThreadpoolCallbackPool(&m_cbe, m_ptpPool);
}

CWindowsThreadPool::~CWindowsThreadPool()
{
	if (m_ptpPool)
	{
		//这个地方会有内存泄漏，特别是对于Timer，如果重发了一个任务，但是任务还没被线程获取，闭包就不能被消除
		//唯一的解法就是要多开一个线程来管理所有的闭包对象
		CloseThreadpool(m_ptpPool);
		::DestroyThreadpoolEnvironment(&m_cbe);
	}
}

void CWindowsThreadPool::SubmitThreadpoolWork(const std::function<void()> &callback)
{
	auto&& item = CThreadLoopItem::MakeItem<CThreadLoopItem>(callback);
	auto&& ptpWork = CreateThreadpoolWork(CWindowsThreadPool::PTP_WORK_CALLBACK_impl, item, &m_cbe);

	::SubmitThreadpoolWork(ptpWork);
}

BOOL CWindowsThreadPool::SetThreadpoolSerialTimerEx(const std::function<void()> &callback, 
											  PFILETIME pftDueTime, DWORD msWindowLength, DWORD nPeriod)
{
	auto&& item = CThreadLoopItem::MakeItem<CThreadLoopTimerItem>(callback, nPeriod);
	auto&& ptpTimer = CreateThreadpoolTimer(CWindowsThreadPool::PTP_SERIALTIMER_CALLBACK_impl, item, &m_cbe);

	item->SetFtDueTime(pftDueTime);
	item->SetMsWindowLength(msWindowLength);

	return ::SetThreadpoolTimerEx(ptpTimer, pftDueTime, 0, msWindowLength);
}

VOID NTAPI CWindowsThreadPool::PTP_WORK_CALLBACK_impl(
	_Inout_     PTP_CALLBACK_INSTANCE Instance,
	_Inout_opt_ PVOID                 Context,
	_Inout_     PTP_WORK              Work
)
{
	CThreadLoopItem *item = reinterpret_cast<CThreadLoopItem *>(Context);
	item->CallBack();

	if (!item->Release())
	{
		item->Destroy();
		CloseThreadpoolWork(Work);
	}
}

VOID NTAPI CWindowsThreadPool::PTP_SERIALTIMER_CALLBACK_impl(
	_Inout_     PTP_CALLBACK_INSTANCE Instance,
	_Inout_opt_ PVOID                 Context,
	_Inout_     PTP_TIMER             Timer
)
{
	// 用windows自带的线程池的方式很难管理线程池timer对象的生存周期
	// 串行的还好办（如下面这样的做法），如果是并发的，那很难做，只能多开一个线程来销毁闭包
	// 但是还是有内存泄漏
	CThreadLoopTimerItem *item = reinterpret_cast<CThreadLoopTimerItem *>(Context);
	item->CallBack();

	if (!item->Release())
	{
		item->Destroy();
		CloseThreadpoolTimer(Timer);
	}
	else
	{
		auto &&ft = item->GetFtDueTime();
		auto &&msWindowLength = item->GetMsWindowLength();
		::SetThreadpoolTimerEx(Timer, &ft, 0, 0);
	}
}