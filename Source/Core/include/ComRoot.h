#ifndef CORE_ATL_COM_H
#define CORE_ATL_COM_H

#include "ComIntf.h"
#include <type_traits>

#ifndef PPV_ARG
#define PPV_ARG(p)			reinterpret_cast<void**>(p)
#endif

#ifndef PV_ARG
#define PV_ARG(p)			reinterpret_cast<void*>(p)
#endif


template <typename T, typename U>
T CheckedCast(U u)
{
	static_assert(!std::is_same<T, U>::value, "Redundant checked_cast");
#ifdef _DEBUG
	if (!u) return nullptr;
	T t = dynamic_cast<T>(u);
	if (!t) assert(!"Invalid type cast");
	return t;
#else
	return static_cast<T>(u);
#endif
}

#ifndef ReturnIfFalse
#define ReturnIfFalse(bool)		\
	if (!(bool)) { LOG_ERROR(#bool); return false; }
#endif

namespace FTS
{
	typedef BOOL(_IntfEntryFunc)(void* pv, CREFIID criid, void** ppv, UINT32 dw);

	struct FIntfMapEntry
	{
		const IID* piid;
		UINT32 dwOffset;
		_IntfEntryFunc* pFunc;
	};
}


#ifndef _SIMPLEINTFENTRY
#define _SIMPLEINTFENTRY ((FTS::_IntfEntryFunc*)1)
#endif

#ifndef __PACKING
#define __PACKING	0x10000000
#endif

#ifndef OFFSET_OF_CLASS
#define OFFSET_OF_CLASS(Base, Derived)	((FTS::UINT64)(static_cast<Base*>((Derived*)__PACKING)) - __PACKING)
#endif

#ifndef OFFSET_OF_AGGREGATION
#define OFFSET_OF_AGGREGATION(a, p)		((FTS::UINT64)&reinterpret_cast<char const volatile&>((((a*)0)->p)))
#endif


#ifndef BEGIN_INTERFACE_MAP
#define BEGIN_INTERFACE_MAP(x)															\
	typedef x	_MapClass;																\
	BOOL _InternalQueryInterface(CREFIID criid, void** ppv) noexcept				\
	{\
		return this->InternalQueryInterface(this, _GetEntries(), criid, ppv);			\
	}\
	virtual UINT32 AddRef() override \
	{ \
		assert(m_dwRefCount != MAX_UINT32); \
		return FComMultiThreadModel::Increment(&m_dwRefCount); \
	} \
	virtual UINT32 Release() override\
	{\
		UINT32 dwCurrRef = FComMultiThreadModel::Decrement(&m_dwRefCount); \
		if (dwCurrRef >= MAX_UINT32 / 4) assert(0 && TEXT("This interface pointer has already been released.")); \
		else if (dwCurrRef == 0) delete this; \
		return dwCurrRef; \
	}\
	virtual BOOL QueryInterface(CREFIID criid, void** ppv) override \
	{ \
		return this->_InternalQueryInterface(criid, ppv); \
	} \
	const static FTS::FIntfMapEntry* _GetEntries() noexcept								\
	{\
		static const FTS::FIntfMapEntry pEntries[] = {
#endif

// 一定要按继承顺序写
#ifndef INTERFACE_ENTRY
#define	INTERFACE_ENTRY(iid, x)							\
	{													\
		&iid,											\
		(UINT32)OFFSET_OF_CLASS(x, _MapClass),			\
		_SIMPLEINTFENTRY								\
	},
#endif


#ifndef END_INTERFACE_MAP
#define END_INTERFACE_MAP									\
	{ nullptr, 0, 0 }}; return &pEntries[0]; }				
#endif


namespace FTS
{
	inline BOOL InlineInternalQueryInterface(
		void* pThis,
		const FIntfMapEntry* pEntries,
		CREFIID criid,
		void** ppv
	) noexcept
	{
		assert(pThis != nullptr);
		assert(pEntries != nullptr);
		assert(pEntries->pFunc == _SIMPLEINTFENTRY);

		if (IsEqualIUnknown(criid))
		{
			IUnknown* pIUnk = (IUnknown*)((UINT64)pThis + pEntries->dwOffset);
			*ppv = pIUnk;

			return true;
		}

		BOOL hRes;
		while (true)
		{
			if (pEntries->pFunc == nullptr)
			{
				hRes = false;
				break;
			}

			BOOL bBlind = pEntries->piid == nullptr;
			if (bBlind || IsEqualIIDs(*(pEntries->piid), criid))
			{
				if (pEntries->pFunc == _SIMPLEINTFENTRY)
				{
					assert(!bBlind);

					IUnknown* IUnk = (IUnknown*)((UINT64)pThis + pEntries->dwOffset);
					*ppv = IUnk;

					return true;
				}

				hRes = pEntries->pFunc(pThis, criid, ppv, pEntries->dwOffset);
				if (hRes == true)
				{
					return true;
				}
				else if (!bBlind && !hRes)
				{
					break;
				}

			}
			pEntries++;
		}

		ppv = nullptr;
		return hRes;
	}

	class FComFakeCriticalSection
	{
	public:
		BOOL Init() noexcept
		{
			return true;
		}

		BOOL Term() noexcept
		{
			return true;
		}

		BOOL Lock() noexcept
		{
			return true;
		}

		BOOL TryLock() noexcept
		{
			return true;
		}

		BOOL Unlock() noexcept
		{
			return true;
		}
	};

	class FComCriticalSection
	{
	public:
		FComCriticalSection() { memset(&m_Sec, 0, sizeof(CriticalSection)); }
		~FComCriticalSection() {}

		BOOL Lock() noexcept
		{
			EnterCritSection(&m_Sec);
			return true;
		}

		BOOL Trylock() noexcept
		{
			if (!TryEnterCritSection(&m_Sec))
			{
				return false;
			}
			return true;
		}

		BOOL Unlock() noexcept
		{
			LeaveCritSection(&m_Sec);
			return true;
		}

		BOOL Init() noexcept
		{
			if (!InitCritSection(&m_Sec))
			{
				return false;
			}
			return true;
		}

		BOOL Term() noexcept
		{
			TermCritSection(&m_Sec);
			return true;
		}

	private:
		CriticalSection m_Sec;
	};

	class FComSafeDeleteCriticalSection : public FComCriticalSection
	{
	public:
		FComSafeDeleteCriticalSection() : m_bInitialized(false) {}
		~FComSafeDeleteCriticalSection()
		{
			if (!m_bInitialized)
			{
				return;
			}
			m_bInitialized = false;
			FComCriticalSection::Term();
		}

		BOOL Init()
		{
			assert(!m_bInitialized);

			BOOL hRes = FComCriticalSection::Init();
			if (hRes)
			{
				m_bInitialized = true;
			}
			return hRes;
		}

		BOOL Term() noexcept
		{
			if (!m_bInitialized)
			{
				return true;
			}
			m_bInitialized = false;
			return FComCriticalSection::Term();
		}

		BOOL Lock() noexcept
		{
			assert(m_bInitialized);

			return FComCriticalSection::Lock();
		}

		BOOL Trylock() noexcept
		{
			assert(m_bInitialized);

			return FComCriticalSection::Trylock();
		}

	private:
		BOOL m_bInitialized;
	};


	class FComSingleThreadModel
	{
	public:
		static UINT32 Increment(UINT32* pdw) noexcept
		{
			return ++(*pdw);
		}

		static UINT32 Decrement(UINT32* pdw) noexcept
		{
			return --(*pdw);
		}

		typedef FComFakeCriticalSection	CriticalSection;
		typedef FComFakeCriticalSection	AutoDeleteCriticalSection;
	};

	class FComMultiThreadModel
	{
	public:
		static UINT32 Increment(UINT32* pdw) noexcept
		{
			return LockedIncrement(pdw);
		}

		static UINT32 Decrement(UINT32* pdw) noexcept
		{
			return LockedDecrement(pdw);
		}

		typedef FComCriticalSection				CriticalSection;
		typedef FComSafeDeleteCriticalSection	AutoDeleteCriticalSection;
	};

	template <class ThreadModel>
	class TComObjectRoot
	{
	public:
		typedef ThreadModel											_ThreadModel;
		typedef typename _ThreadModel::CriticalSection				_CritSec;
		typedef typename _ThreadModel::AutoDeleteCriticalSection	_AutoDelCritSec;

		TComObjectRoot() : m_dwRefCount(1) 
		{
			m_CritSec.Init(); 
		}

		static BOOL InternalQueryInterface(void* pThis, const FIntfMapEntry* pEntries, CREFIID criid, void** ppv)
		{
			assert(ppv != nullptr);
			assert(pThis != nullptr);

			assert(pEntries->pFunc == _SIMPLEINTFENTRY);

			return InlineInternalQueryInterface(pThis, pEntries, criid, ppv);
		}

		BOOL Lock()
		{
			return m_CritSec.Lock();
		}

		BOOL Trylock()
		{
			return m_CritSec.Trylock();
		}

		BOOL Unlock()
		{
			return m_CritSec.Unlock();
		}

	protected:
		_AutoDelCritSec m_CritSec;

		UINT32 m_dwRefCount;
	};


	template <>
	class TComObjectRoot<FComSingleThreadModel>
	{
	public:
		typedef FComSingleThreadModel					_ThreadModel;
		typedef _ThreadModel::CriticalSection			_CritSec;
		typedef _ThreadModel::AutoDeleteCriticalSection	_AutoDelCritSec;

		TComObjectRoot() : m_dwRefCount(1) 
		{  
		}

		static BOOL InternalQueryInterface(void* pThis, const FIntfMapEntry* pEntries, CREFIID criid, void** ppv)
		{
			assert(ppv != nullptr);
			assert(pThis != nullptr);

			assert(pEntries->pFunc == _SIMPLEINTFENTRY);

			return InlineInternalQueryInterface(pThis, pEntries, criid, ppv);
		}

		BOOL Lock()
		{
			return true;
		}

		BOOL Trylock()
		{
			return true;
		}

		BOOL Unlock()
		{
			return true;
		}

	protected:
		UINT32 m_dwRefCount;
	};
}

#endif