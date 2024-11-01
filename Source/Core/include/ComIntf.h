#ifndef CORE_COM_INF_H
#define CORE_COM_INF_H

#include "SysCall.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <string>
#include <unknwnbase.h>

namespace FTS
{
	struct IID
	{
		UINT32  Data1;
		UINT16 Data2;
		UINT16 Data3;
		UINT8  Data4[8];
	};

};

#ifndef CREFIID
#define CREFIID const IID&
#endif


namespace FTS
{
	inline BOOL IsEqualIUnknown(CREFIID criid)
	{
		return (
			((UINT32*)&criid)[0] == 0 &&
			((UINT32*)&criid)[1] == 0 &&
			((UINT32*)&criid)[2] == 0 &&
			((UINT32*)&criid)[3] == 0
		);
	}

	inline BOOL IsEqualIIDs(CREFIID criid1, CREFIID criid2)
	{
		return 
			criid1.Data1 == criid2.Data1 &&
			criid1.Data2 == criid2.Data2 &&
			criid1.Data3 == criid2.Data3 &&
			((UINT64*)criid1.Data4)[0] == ((UINT64*)criid2.Data4)[0] 
		;
	}


	inline static constexpr IID IID_IUnknown = { 0x00000000, 0x0000 };
	struct IUnknown
	{
		virtual UINT32 AddRef() = 0;
		virtual UINT32 Release() = 0;
		virtual BOOL QueryInterface(CREFIID criid, void** ppv) = 0;

		virtual ~IUnknown() = default;
	};

	static std::string LogString(std::string str, std::string strFile, UINT32 dwLine)
	{
		std::stringstream ss;
		ss << str << "   [File: " << strFile << "(" << dwLine << ")]";
		return ss.str();
	}

#ifndef LOG_INFO
#define LOG_INFO(x)	spdlog::info(FTS::LogString(x, __FILE__, __LINE__));
#endif
#ifndef LOG_WARN
#define LOG_WARN(x)	spdlog::warn(FTS::LogString(x, __FILE__, __LINE__));
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(x) spdlog::error(FTS::LogString(x, __FILE__, __LINE__));
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(x)	spdlog::critical(FTS::LogString(x, __FILE__, __LINE__));
#endif


}

#endif
