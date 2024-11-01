#ifndef CORE_COM_CLI_H
#define CORE_COM_CLI_H


#include "ComIntf.h"
#include <type_traits>

namespace FTS
{
	template <class T>
	class TComPtr
	{
	protected:

		UINT32 InternalAddRef() const noexcept
		{
			if (m_p != nullptr)
			{
				return m_p->AddRef();
			}
			return 0;
		}

		UINT32 InternalRelease()
		{
			UINT32 dwRef = 0;
			if (m_p != nullptr)
			{
				dwRef = m_p->Release();
				m_p = nullptr;
			}

			return dwRef;
		}

	public:
		typedef T	_InterfaceType;

		TComPtr() noexcept : m_p(nullptr)
		{
		}

		TComPtr(_InterfaceType* p) noexcept : m_p(p)
		{
			InternalAddRef();
		}

		TComPtr(const TComPtr& crOther) noexcept : m_p(crOther.m_p)
		{
			InternalAddRef();
		}

		template<class U>
		requires std::is_convertible_v<U*, T*>
        TComPtr(const TComPtr<U>& crOther) noexcept : m_p(crOther.Get())
        {
            InternalAddRef();
        }

		TComPtr(TComPtr&& rrOther) noexcept : m_p(nullptr)
		{
			if (this != reinterpret_cast<TComPtr*>(&reinterpret_cast<UINT8&>(rrOther)))
			{
				Swap(rrOther);
			}
		}

		~TComPtr() noexcept
		{
			InternalRelease();
		}


		TComPtr& operator=(decltype(nullptr)) noexcept
		{
			InternalRelease();
			return *this;
		}

		TComPtr& operator=(_InterfaceType* pOther) noexcept
		{
			if (m_p != pOther)
			{
				TComPtr(pOther).Swap(*this);
			}
			return *this;
		}

		TComPtr& operator=(const TComPtr& crOther) noexcept
		{
			if (m_p != crOther.m_p)
			{
				TComPtr(crOther).Swap(*this);
			}
			return *this;
		}

		template<class U>
		requires std::is_convertible_v<U*, T*>
        TComPtr& operator=(const TComPtr<U>& crOther) noexcept
        {
            TComPtr<_InterfaceType>(crOther).Swap(*this);
            return *this;
        }

		TComPtr& operator=(TComPtr&& rrOther) noexcept
		{
			TComPtr(static_cast<TComPtr&&>(rrOther)).Swap(*this);
			return *this;
		}

		_InterfaceType* operator->() const noexcept
		{
			return m_p;
		}

		BOOL operator==(T* pOther) const noexcept
		{
			return m_p == pOther;
		}

		BOOL operator!=(T* pOther) const noexcept
		{
			return m_p != pOther;
		}

		BOOL operator==(const TComPtr& crOther) const noexcept
		{
			return m_p == crOther.m_p;
		}

		BOOL operator==(decltype(nullptr)) const  noexcept
		{
			return m_p == nullptr;
		}

		BOOL operator!=(decltype(nullptr)) const  noexcept
		{
			return m_p != nullptr;
		}

		BOOL operator!=(const TComPtr& crOther) const noexcept
		{
			return !((*this) == crOther);
		}

		operator BOOL() const
		{
			return m_p != nullptr;
		}

		_InterfaceType** operator&() noexcept
		{
			return &m_p;
		}
		
		
		_InterfaceType* Get() const noexcept
		{
			return m_p;
		}

		_InterfaceType** GetAddressOf() noexcept
		{
			return &m_p;
		}

		_InterfaceType* const* GetAddressOf() const noexcept
		{
			return &m_p;
		}

		_InterfaceType** ReleaseAndGetAddressOf() noexcept
		{
			InternalRelease();
			return &m_p;
		}

		void Swap(TComPtr& rOther) noexcept
		{
			_InterfaceType* pTemp = m_p;
			m_p = rOther.m_p;
			rOther.m_p = pTemp;
		}

		void Swap(TComPtr&& rrOther) noexcept
		{
			_InterfaceType* pTemp = m_p;
			m_p = rrOther.m_p;
			rrOther.m_p = pTemp;
		}

		_InterfaceType* Detach() noexcept
		{
			_InterfaceType* pTemp = m_p;
			m_p = nullptr;
			return pTemp;
		}

		void Attach(_InterfaceType* p) noexcept
		{
			if (m_p != nullptr)
			{
				UINT32 dwRef = m_p->Release();
				FTSASSERT((dwRef != 0 || m_p != p) && TEXT("ComPtr can't attach a released interface pointer."));
			}
			m_p = p;
		}

		UINT32 Reset() noexcept
		{
			return InternalRelease();
		}

		BOOL CopyTo(_InterfaceType** pp) const noexcept
		{
			BOOL hRes = false;
			if (pp != nullptr)
			{
				InternalAddRef();
				*pp = m_p;
				hRes = true;
			}
			return hRes;
		}

		BOOL CopyTo(CREFIID criid, void** ppv) const noexcept
		{
			return m_p->QueryInterface(criid, ppv);
		}

		BOOL As(CREFIID criid, TComPtr<IUnknown>* pIUnkPtr) const noexcept
		{
			return m_p->QueryInterface(criid, reinterpret_cast<void**>(pIUnkPtr->ReleaseAndGetAddressOf()));
		}

	private:
		_InterfaceType* m_p;
	};

}



#endif