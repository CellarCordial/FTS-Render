#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H


#include "../../Math/include/Vector.h"
#include "../../Math/include/Matrix.h"
#include "../../Math/include/Bounds.h"
#include "../../Math/include/Bvh.h"
#include "../../Core/include/Entity.h"
#include "Image.h"
#include <basetsd.h>
#include <vector>


namespace FTS 
{
    struct FVertex
    {
        FVector3F Position;
        FVector3F Normal;
        FVector4F Tangent;
        FVector2F UV;
    };

    struct FMaterial
    {
        enum
        {
            TextureType_Diffuse,
            TextureType_Normal,
            TextureType_Emissive,
            TextureType_Occlusion,
            TextureType_MetallicRoughness,
            TextureType_Num
        };

        struct SubMaterial
        {
			FLOAT fDiffuseFactor[4] = { 0.0f };
			FLOAT fRoughnessFactor = 0.0f;
			FLOAT fMetallicFactor = 0.0f;
			FLOAT fOcclusionFactor = 0.0f;
			FLOAT fEmissiveFactor[3] = { 0.0f };

            FImage Images[TextureType_Num];

            BOOL operator==(const SubMaterial& crOther) const
            {
                ReturnIfFalse(  
                    fDiffuseFactor[0] == crOther.fDiffuseFactor[0] &&
                    fDiffuseFactor[1] == crOther.fDiffuseFactor[1] &&
                    fDiffuseFactor[2] == crOther.fDiffuseFactor[2] &&
                    fDiffuseFactor[3] == crOther.fDiffuseFactor[3] &&
                    fRoughnessFactor  == crOther.fRoughnessFactor &&
                    fMetallicFactor   == crOther.fMetallicFactor &&
                    fOcclusionFactor  == crOther.fOcclusionFactor &&
                    fEmissiveFactor[0]   == crOther.fEmissiveFactor[0] &&
                    fEmissiveFactor[1]   == crOther.fEmissiveFactor[1] &&
                    fEmissiveFactor[2]   == crOther.fEmissiveFactor[2]
                );

                for (UINT32 ix = 0; ix < TextureType_Num; ++ix)
                {
                    ReturnIfFalse(Images[ix] == crOther.Images[ix]);
                }
                return true;            
            }

            BOOL operator!=(const SubMaterial& crOther) const
            {
                return !((*this) == crOther);
            }
        };

        std::vector<SubMaterial> SubMaterials;

        BOOL operator==(const FMaterial& crOther) const
        {
            ReturnIfFalse(SubMaterials.size() == crOther.SubMaterials.size());
            for (UINT64 ix = 0; ix < SubMaterials.size(); ++ix)
            {
                ReturnIfFalse(SubMaterials[ix] == crOther.SubMaterials[ix]);
            }
            return true;
        }
    };

    struct FMesh
    {
        struct SubMesh
        {
            std::vector<FVertex> Vertices;
            std::vector<UINT32> Indices;
            
            FMatrix4x4 WorldMatrix;
            UINT32 dwMaterialIndex;
        };

        std::vector<SubMesh> SubMeshes;
        FMatrix4x4 WorldMatrix;
        BOOL bCulling = false;
    };



    namespace Geometry
    {
        FMesh CreateBox(FLOAT fWidth, FLOAT fHeight, FLOAT fDepth, UINT32 dwSubdivisionsNum);
        FMesh CreateSphere(FLOAT radius, UINT32 sliceCount, UINT32 stackCount);
        FMesh CreateGeosphere(FLOAT radius, UINT32 numSubdivisions);
        FMesh CreateCylinder(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount);
        FMesh CreateGrid(FLOAT width, FLOAT depth, UINT32 m, UINT32 n);
        FMesh CreateQuad(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT depth);
        void Subdivide(FMesh::SubMesh& rMeshData);
        FVertex MidPoint(const FVertex& v0, const FVertex& v1);
        void BuildCylinderTopCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::SubMesh& meshData);
        void BuildCylinderBottomCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::SubMesh& meshData);
    };
    
}

















#endif