#ifndef CORE_SYS_CALL_H
#define CORE_SYS_CALL_H

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#endif
#include <string>
#include <cassert>

namespace FTS
{
	extern "C"
	{
		typedef signed char         INT8;
		typedef signed short        INT16;
		typedef signed int          INT32;
		typedef signed long long    INT64;
		typedef unsigned char       UINT8;
		typedef unsigned short      UINT16;
		typedef unsigned int        UINT32;
		typedef unsigned long long	UINT64;

		typedef signed long			LONG;
		typedef unsigned long		ULONG;
		typedef signed long long	LLONG;
		typedef unsigned long long	ULLONG;

		typedef float				FLOAT;
		typedef double				DOUBLE;
		typedef long double			LDOUBLE;

		typedef bool				BOOL;

		typedef char				CHAR;
		typedef wchar_t				WCHAR;
		typedef size_t				SIZE_T;
	}
}


#ifdef _MSC_VER
#define NO_VTABLE __declspec(novtable)
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#else
#define NO_VTABLE
#define DLL_EXPORT
#define DLL_IMPORT
#endif

#ifndef _INFINITY_
#define _INFINITY_ std::numeric_limits<FLOAT>::infinity()
#endif

#ifndef TEXT
#define TEXT		L##x
#endif

#ifndef MAX_UINT8
#define MAX_UINT8   ((UINT8)~((UINT8)0))
#endif
#ifndef MAX_INT8
#define MAX_INT8    ((INT8)(MAX_UINT8 >> 1))
#endif
#ifndef MIN_INT8
#define MIN_INT8    ((INT8)~MAX_INT8)
#endif

#ifndef MAX_UINT16
#define MAX_UINT16  ((UINT16)~((UINT16)0))
#endif
#ifndef MAX_INT16
#define MAX_INT16   ((INT16)(MAX_UINT16 >> 1))
#endif
#ifndef MIN_INT16
#define MIN_INT16   ((INT16)~MAX_INT16)
#endif

#ifndef MAX_UINT32
#define MAX_UINT32  ((UINT32)~((UINT32)0))
#endif
#ifndef MAX_INT32
#define MAX_INT32   ((INT32)(MAX_UINT32 >> 1))
#endif
#ifndef MIN_INT32
#define MIN_INT32   ((INT32)~MAX_INT32)
#endif

#ifndef MAX_UINT64
#define MAX_UINT64  ((UINT64)~((UINT64)0))
#endif
#ifndef MAX_INT64
#define MAX_INT64   ((INT64)(MAX_UINT64 >> 1))
#endif
#ifndef MIN_INT64
#define MIN_INT64   ((INT64)~MAX_INT64)
#endif

#ifndef ReturnIfFailed
#define ReturnIfFailed(hRes)	\
	if (FAILED(hRes))	return hRes 
#endif

#ifndef new_on_stack
#define new_on_stack(T) (T*)alloca(sizeof(T))
#endif

namespace FTS
{
	inline std::string WStringToString(const std::wstring& WString)
	{
		if (WString.empty())
		{
			return std::string("");
		}

		int Size = WideCharToMultiByte(CP_UTF8, 0, &WString[0], (int)WString.size(), NULL, 0, NULL, NULL);
		std::string String(Size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &WString[0], (int)WString.size(), &String[0], Size, NULL, NULL);
		return String;
	}

	inline std::wstring StringToWString(const std::string& String)
	{
		if (String.empty())
		{
			return std::wstring(L"");
		}

		int Size = MultiByteToWideChar(CP_UTF8, 0, &String[0], (int)String.size(), NULL, 0);
		std::wstring WString(Size, 0);
		MultiByteToWideChar(CP_UTF8, 0, &String[0], (int)WString.size(), &WString[0], Size);
		return WString;
	}

	inline UINT32 LockedIncrement(UINT32* pdw) noexcept
	{
#if defined(WIN32) || defined(_WIN32)
		return ::InterlockedIncrement(pdw);
#endif
	}

	inline UINT32 LockedDecrement(UINT32* pdw) noexcept
	{
#if defined(WIN32) || defined(_WIN32)
		return ::InterlockedDecrement(pdw);
#endif
	}


#if defined(WIN32) || defined(_WIN32)

	typedef CRITICAL_SECTION CriticalSection;

#endif

	inline void EnterCritSection(CriticalSection* pSec)
	{
#if defined(WIN32) || defined(_WIN32)
		EnterCriticalSection(pSec);
#endif
	}

	inline BOOL TryEnterCritSection(CriticalSection* pSec)
	{
#if defined(WIN32) || defined(_WIN32)
		return TryEnterCriticalSection(pSec);
#endif
	}

	inline void LeaveCritSection(CriticalSection* pSec)
	{
#if defined(WIN32) || defined(_WIN32)
		LeaveCriticalSection(pSec);
#endif
	}

	inline BOOL InitCritSection(CriticalSection* pSec)
	{
#if defined(WIN32) || defined(_WIN32)
		return InitializeCriticalSectionEx(pSec, 0, 0);
#endif
	}

	inline void TermCritSection(CriticalSection* pSec)
	{
#if defined(WIN32) || defined(_WIN32)
		DeleteCriticalSection(pSec);
#endif
	}
}

#endif
