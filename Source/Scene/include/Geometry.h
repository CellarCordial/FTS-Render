#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H


#include "../../Math/include/Vector.h"
#include "../../Math/include/Matrix.h"
#include "../../Math/include/Surface.h"
#include "../../Core/include/ComRoot.h"
#include "../../Tools/include/HashTable.h"
#include "../../Tools/include/BitAllocator.h"
#include "Image.h"
#include <basetsd.h>


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
        struct Submesh
        {
            std::vector<FVertex> Vertices;
            std::vector<UINT32> Indices;
            
            FMatrix4x4 WorldMatrix;
            UINT32 dwMaterialIndex;
        };

        std::vector<Submesh> Submeshes;
        FMatrix4x4 WorldMatrix;
        BOOL bCulling = false;
    };

    class FMeshOptimizer
    {
    public:
        void Optimize(UINT32 dwTargetTriangleNum);
        void LockPosition(FVector3F Pos);


        FLOAT fMaxError;
        UINT32 dwRemainVertexNum;
        UINT32 dwRemainTriangleNum;

    private:


    private:
        enum
        {
            AdjacencyMask   = 1,
            LockMask        = 2
        };
        std::vector<UINT8> m_Masks;

        std::vector<FVector3F> m_Vertices;
        std::vector<UINT32> m_Indices;
        std::vector<FQuadricSurface> m_TriangleSurfaces;

        FBitSetAllocator m_bTrangleRemovedArray;

        FHashTable m_VertexTable;
        FHashTable m_CornerTable;


        std::vector<std::pair<FVector3F, FVector3F>> m_Edges;
        FHashTable m_EdgeBeginTable;
        FHashTable m_EdgeEndTable;


        std::vector<UINT32> m_MovedVertexIndices;
        std::vector<UINT32> m_MovedIndexIndices;
        std::vector<UINT32> m_MovedEdgeIndices;
        std::vector<UINT32> m_EdgeNeedReevaluateIndices;
    };


    namespace Geometry
    {
        FMesh CreateBox(FLOAT fWidth, FLOAT fHeight, FLOAT fDepth, UINT32 dwSubdivisionsNum);
        FMesh CreateSphere(FLOAT radius, UINT32 sliceCount, UINT32 stackCount);
        FMesh CreateGeosphere(FLOAT radius, UINT32 numSubdivisions);
        FMesh CreateCylinder(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount);
        FMesh CreateGrid(FLOAT width, FLOAT depth, UINT32 m, UINT32 n);
        FMesh CreateQuad(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT depth);
        void Subdivide(FMesh::Submesh& rMeshData);
        FVertex MidPoint(const FVertex& v0, const FVertex& v1);
        void BuildCylinderTopCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::Submesh& meshData);
        void BuildCylinderBottomCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::Submesh& meshData);
    };
    
}

















#endif