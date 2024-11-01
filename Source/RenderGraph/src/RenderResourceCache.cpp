#include "RenderResourceCache.h"
#include <string>


namespace FTS
{
    BOOL FRenderResourceCache::Initialize() 
    {
        ReturnIfFalse(m_pWorld != nullptr);
        return true;
    }

    
    BOOL FRenderResourceCache::Collect(IResource* pResource)
    {
        ReturnIfFalse(pResource != nullptr);

        IBuffer* pBuffer;
        ITexture* pTexture;
        ISampler* pSampler;
        if (pResource->QueryInterface(IID_IBuffer, PPV_ARG(&pBuffer)))
        {
            FBufferDesc Desc = pBuffer->GetDesc();
            ReturnIfFalse(!Desc.strName.empty());
            m_ResourceNameMap[Desc.strName] = ResourceData{ .pResource = pResource };
        }
        else if (pResource->QueryInterface(IID_ITexture, PPV_ARG(&pTexture)))
        {
            FTextureDesc Desc = pTexture->GetDesc();
			ReturnIfFalse(!Desc.strName.empty());
            m_ResourceNameMap[Desc.strName] = ResourceData{ .pResource = pResource };
        }
        else if (pResource->QueryInterface(IID_ISampler, PPV_ARG(&pSampler)))
        {
            FSamplerDesc Desc = pSampler->GetDesc();
			ReturnIfFalse(!Desc.strName.empty());
            m_ResourceNameMap[Desc.strName] = ResourceData{ .pResource = pResource };
        }
        else 
        {
            LOG_ERROR("Render graph collect resource failed.");
            return false;
        }

        return true;
    }

    IResource* FRenderResourceCache::Require(const CHAR* pstrName)
    {
        auto Iter = m_ResourceNameMap.find(std::string(pstrName));
        if (Iter != m_ResourceNameMap.end())
        {
            return Iter->second.pResource.Get();
        }

        std::string str = "There is no resource named ";
        LOG_ERROR(str + std::string(pstrName));
        return nullptr;
    }
    
	BOOL FRenderResourceCache::CollectConstants(const CHAR* strName, void* pvData, UINT64 stElementNum)
	{
		if (pvData == nullptr || strName == nullptr)
		{
			LOG_ERROR("Collect constants has a null pointer.");
			return true;
		}

		if (m_DataNameMap.find(std::string(strName)) == m_DataNameMap.end())
		{
			m_DataNameMap[std::string(strName)] = std::make_pair(pvData, stElementNum);
			return true;
		}

		LOG_ERROR("Render resource cache already has the same constant data.");
		return false;
	}

	BOOL FRenderResourceCache::RequireConstants(const CHAR* strName, void** ppvData, UINT64* pstElementNum)
	{
		if (strName == nullptr || ppvData == nullptr)
		{
			LOG_ERROR("Collect constants has a null pointer.");
			return false;
		}

		auto Iter = m_DataNameMap.find(std::string(strName));

		if (Iter == m_DataNameMap.end())
		{
			std::string str = "Render resource cache doesn't have the constant data called ";
			str += strName;
			LOG_ERROR(str);
			return false;
		}

        *ppvData = Iter->second.first;
        if (pstElementNum) *pstElementNum = Iter->second.second;

        return true;
	}
}