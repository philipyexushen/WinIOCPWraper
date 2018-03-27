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
		//����ط������ڴ�й©���ر��Ƕ���Timer������ط���һ�����񣬵�������û���̻߳�ȡ���հ��Ͳ��ܱ�����
		//Ψһ�Ľⷨ����Ҫ�࿪һ���߳����������еıհ�����
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
	// ��windows�Դ����̳߳صķ�ʽ���ѹ����̳߳�timer�������������
	// ���еĻ��ð죨������������������������ǲ����ģ��Ǻ�������ֻ�ܶ࿪һ���߳������ٱհ�
	// ���ǻ������ڴ�й©
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