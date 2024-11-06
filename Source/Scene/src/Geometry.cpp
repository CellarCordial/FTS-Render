#include "../include/Geometry.h"


#include <memory>
#include <string>
namespace FTS
{

	namespace Geometry
	{
		FMesh CreateBox(FLOAT width, FLOAT height, FLOAT depth, UINT32 numSubdivisions)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();

			FVertex v[24];

			FLOAT w2 = 0.5f * width;
			FLOAT h2 = 0.5f * height;
			FLOAT d2 = 0.5f * depth;

			v[0] = FVertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[1] = FVertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[2] = FVertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[3] = FVertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[4] = FVertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[5] = FVertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[6] = FVertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[7] = FVertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[8] = FVertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[9] = FVertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[10] = FVertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[11] = FVertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[12] = FVertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[13] = FVertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[14] = FVertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[15] = FVertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[16] = FVertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 1.0f });
			v[17] = FVertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f });
			v[18] = FVertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f });
			v[19] = FVertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 1.0f });
			v[20] = FVertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f });
			v[21] = FVertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f });
			v[22] = FVertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f });
			v[23] = FVertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f });

			rSubMesh.Vertices.assign(&v[0], &v[24]);

			UINT16 i[36];

			// Fill in the front face index data
			i[0] = 0; i[1] = 1; i[2] = 2;
			i[3] = 0; i[4] = 2; i[5] = 3;

			// Fill in the back face index data
			i[6] = 4; i[7] = 5; i[8] = 6;
			i[9] = 4; i[10] = 6; i[11] = 7;

			// Fill in the top face index data
			i[12] = 8; i[13] = 9; i[14] = 10;
			i[15] = 8; i[16] = 10; i[17] = 11;

			// Fill in the bottom face index data
			i[18] = 12; i[19] = 13; i[20] = 14;
			i[21] = 12; i[22] = 14; i[23] = 15;

			// Fill in the left face index data
			i[24] = 16; i[25] = 17; i[26] = 18;
			i[27] = 16; i[28] = 18; i[29] = 19;

			// Fill in the right face index data
			i[30] = 20; i[31] = 21; i[32] = 22;
			i[33] = 20; i[34] = 22; i[35] = 23;

			rSubMesh.Indices.assign(&i[0], &i[36]);

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<UINT32>(numSubdivisions, 6u);

			for (UINT32 i = 0; i < numSubdivisions; ++i) Subdivide(rSubMesh);

			return Mesh;
		}

		void Subdivide(FMesh::SubMesh& meshData)
		{
			FMesh::SubMesh inputCopy = meshData;


			meshData.Vertices.resize(0);
			meshData.Indices.resize(0);

			//       v1
			//       *
			//      / \
            //     /   \
            //  m0*-----*m1
			//   / \   / \
            //  /   \ /   \
            // *-----*-----*
			// v0    m2     v2

			UINT32 numTris = (UINT32)inputCopy.Indices.size() / 3;
			for (UINT32 i = 0; i < numTris; ++i)
			{
				FVertex v0 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 0]];
				FVertex v1 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 1]];
				FVertex v2 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 2]];

				//
				// Generate the midpoints.
				//

				FVertex m0 = MidPoint(v0, v1);
				FVertex m1 = MidPoint(v1, v2);
				FVertex m2 = MidPoint(v0, v2);

				//
				// Add new geometry.
				//

				meshData.Vertices.push_back(v0); // 0
				meshData.Vertices.push_back(v1); // 1
				meshData.Vertices.push_back(v2); // 2
				meshData.Vertices.push_back(m0); // 3
				meshData.Vertices.push_back(m1); // 4
				meshData.Vertices.push_back(m2); // 5

				meshData.Indices.push_back(i * 6 + 0);
				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 5);

				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 4);
				meshData.Indices.push_back(i * 6 + 5);

				meshData.Indices.push_back(i * 6 + 5);
				meshData.Indices.push_back(i * 6 + 4);
				meshData.Indices.push_back(i * 6 + 2);

				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 1);
				meshData.Indices.push_back(i * 6 + 4);
			}
		}

		FVertex MidPoint(const FVertex& v0, const FVertex& v1)
		{
			FVector3F pos = 0.5f * (v0.Position + v1.Position);
			FVector3F normal = Normalize(0.5f * (v0.Normal + v1.Normal));
			FVector4F tangent = Normalize(0.5f * (v0.Tangent + v1.Tangent));
			FVector2F tex = 0.5f * (v0.UV + v1.UV);

			FVertex v;
			v.Position = pos;
			v.Normal = normal;
			v.Tangent = tangent;
			v.UV = tex;

			return v;
		}


		FMesh CreateSphere(FLOAT radius, UINT32 sliceCount, UINT32 stackCount)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();
			//
			// Compute the vertices stating at the top pole and moving down the stacks.
			//

			// Poles: note that there will be texture coordinate distortion as there is
			// not a unique point on the texture map to assign to the pole when mapping
			// a rectangular texture onto a sphere.
			FVertex topVertex({ 0.0f, +radius, 0.0f }, { 0.0f, +1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			FVertex bottomVertex({ 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });

			rSubMesh.Vertices.push_back(topVertex);

			FLOAT phiStep = PI / stackCount;
			FLOAT thetaStep = 2.0f * PI / sliceCount;

			// Compute vertices for each stack ring (do not count the poles as rings).
			for (UINT32 i = 1; i <= stackCount - 1; ++i)
			{
				FLOAT phi = i * phiStep;

				// Vertices of ring.
				for (UINT32 j = 0; j <= sliceCount; ++j)
				{
					FLOAT theta = j * thetaStep;

					FVertex v;

					// spherical to cartesian
					v.Position.x = radius * sinf(phi) * cosf(theta);
					v.Position.y = radius * cosf(phi);
					v.Position.z = radius * sinf(phi) * sinf(theta);

					// Partial derivative of P with respect to theta
					v.Tangent.x = -radius * sinf(phi) * sinf(theta);
					v.Tangent.y = 0.0f;
					v.Tangent.z = +radius * sinf(phi) * cosf(theta);

					v.Tangent = Normalize(v.Tangent);
					v.Normal = Normalize(v.Normal);

					v.UV.x = theta / 2.0f * PI;
					v.UV.y = phi / PI;

					rSubMesh.Vertices.push_back(v);
				}
			}

			rSubMesh.Vertices.push_back(bottomVertex);

			//
			// Compute indices for top stack.  The top stack was written first to the FVertex buffer
			// and connects the top pole to the first ring.
			//

			for (UINT32 i = 1; i <= sliceCount; ++i)
			{
				rSubMesh.Indices.push_back(0);
				rSubMesh.Indices.push_back(i + 1);
				rSubMesh.Indices.push_back(i);
			}

			//
			// Compute indices for inner stacks (not connected to poles).
			//

			// Offset the indices to the index of the first FVertex in the first ring.
			// This is just skipping the top pole FVertex.
			UINT32 baseIndex = 1;
			UINT32 ringFVerUVount = sliceCount + 1;
			for (UINT32 i = 0; i < stackCount - 2; ++i)
			{
				for (UINT32 j = 0; j < sliceCount; ++j)
				{
					rSubMesh.Indices.push_back(baseIndex + i * ringFVerUVount + j);
					rSubMesh.Indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					rSubMesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);

					rSubMesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);
					rSubMesh.Indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					rSubMesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j + 1);
				}
			}

			//
			// Compute indices for bottom stack.  The bottom stack was written last to the FVertex buffer
			// and connects the bottom pole to the bottom ring.
			//

			// South pole FVertex was added last.
			UINT32 southPoleIndex = (UINT32)rSubMesh.Vertices.size() - 1;

			// Offset the indices to the index of the first FVertex in the last ring.
			baseIndex = southPoleIndex - ringFVerUVount;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				rSubMesh.Indices.push_back(southPoleIndex);
				rSubMesh.Indices.push_back(baseIndex + i);
				rSubMesh.Indices.push_back(baseIndex + i + 1);
			}

			return Mesh;
		}


		FMesh CreateGeosphere(FLOAT radius, UINT32 numSubdivisions)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<UINT32>(numSubdivisions, 6u);

			// Approximate a sphere by tessellating an icosahedron.

			const FLOAT X = 0.525731f;
			const FLOAT Z = 0.850651f;

			FVector3F pos[12] =
			{
				FVector3F(-X, 0.0f, Z),  FVector3F(X, 0.0f, Z),
				FVector3F(-X, 0.0f, -Z), FVector3F(X, 0.0f, -Z),
				FVector3F(0.0f, Z, X),   FVector3F(0.0f, Z, -X),
				FVector3F(0.0f, -Z, X),  FVector3F(0.0f, -Z, -X),
				FVector3F(Z, X, 0.0f),   FVector3F(-Z, X, 0.0f),
				FVector3F(Z, -X, 0.0f),  FVector3F(-Z, -X, 0.0f)
			};

			UINT32 k[60] =
			{
				1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
				1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
				3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
				10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
			};

			rSubMesh.Vertices.resize(12);
			rSubMesh.Indices.assign(&k[0], &k[60]);

			for (UINT32 i = 0; i < 12; ++i)
				rSubMesh.Vertices[i].Position = pos[i];

			for (UINT32 i = 0; i < numSubdivisions; ++i)
				Subdivide(rSubMesh);

			// Project vertices onto sphere and scale.
			for (UINT32 i = 0; i < rSubMesh.Vertices.size(); ++i)
			{
				// Project onto unit sphere.
				rSubMesh.Vertices[i].Position = Normalize(rSubMesh.Vertices[i].Position);

				// Project onto sphere.
				rSubMesh.Vertices[i].Normal = radius * rSubMesh.Vertices[i].Position;


				// Derive texture coordinates from spherical coordinates.
				FLOAT theta = atan2f(rSubMesh.Vertices[i].Position.z, rSubMesh.Vertices[i].Position.x);

				// Put in [0, 2pi].
				if (theta < 0.0f)
					theta += 2.0f * PI;

				FLOAT phi = acosf(rSubMesh.Vertices[i].Position.y / radius);

				rSubMesh.Vertices[i].UV.x = theta / 2.0f * PI;
				rSubMesh.Vertices[i].UV.y = phi / PI;

				// Partial derivative of P with respect to theta
				rSubMesh.Vertices[i].Tangent.x = -radius * sinf(phi) * sinf(theta);
				rSubMesh.Vertices[i].Tangent.y = 0.0f;
				rSubMesh.Vertices[i].Tangent.z = +radius * sinf(phi) * cosf(theta);

				rSubMesh.Vertices[i].Tangent = Normalize(rSubMesh.Vertices[i].Tangent);
			}

			return Mesh;
		}

		FMesh CreateCylinder(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();

			//
			// Build Stacks.
			// 

			FLOAT stackHeight = height / stackCount;

			// Amount to increment radius as we move up each stack level from bottom to top.
			FLOAT radiusStep = (topRadius - bottomRadius) / stackCount;

			UINT32 ringCount = stackCount + 1;

			// Compute vertices for each stack ring starting at the bottom and moving up.
			for (UINT32 i = 0; i < ringCount; ++i)
			{
				FLOAT y = -0.5f * height + i * stackHeight;
				FLOAT r = bottomRadius + i * radiusStep;

				// vertices of ring
				FLOAT dTheta = 2.0f * PI / sliceCount;
				for (UINT32 j = 0; j <= sliceCount; ++j)
				{
					FVertex vertex;

					FLOAT c = cosf(j * dTheta);
					FLOAT s = sinf(j * dTheta);

					vertex.Position = FVector3F(r * c, y, r * s);

					vertex.UV.x = (FLOAT)j / sliceCount;
					vertex.UV.y = 1.0f - (FLOAT)i / stackCount;

					// Cylinder can be parameterized as follows, where we introduce v
					// parameter that goes in the same direction as the v tex-coord
					// so that the bitangent goes in the same direction as the v tex-coord.
					//   Let r0 be the bottom radius and let r1 be the top radius.
					//   y(v) = h - hv for v in [0,1].
					//   r(v) = r1 + (r0-r1)v
					//
					//   x(t, v) = r(v)*cos(t)
					//   y(t, v) = h - hv
					//   z(t, v) = r(v)*sin(t)
					// 
					//  dx/dt = -r(v)*sin(t)
					//  dy/dt = 0
					//  dz/dt = +r(v)*cos(t)
					//
					//  dx/dv = (r0-r1)*cos(t)
					//  dy/dv = -h
					//  dz/dv = (r0-r1)*sin(t)

					// This is unit length.
					vertex.Tangent = FVector4F(-s, 0.0f, c, 1.0f);

					FLOAT dr = bottomRadius - topRadius;
					FVector4F bitangent(dr * c, -height, dr * s, 1.0f);

					vertex.Normal = Normalize(Cross(FVector3F(vertex.Tangent), FVector3F(bitangent)));
					rSubMesh.Vertices.push_back(vertex);
				}
			}

			// Add one because we duplicate the first and last vertex per ring
			// since the texture coordinates are different.
			UINT32 ringVerUVount = sliceCount + 1;

			// Compute indices for each stack.
			for (UINT32 i = 0; i < stackCount; ++i)
			{
				for (UINT32 j = 0; j < sliceCount; ++j)
				{
					rSubMesh.Indices.push_back(i * ringVerUVount + j);
					rSubMesh.Indices.push_back((i + 1) * ringVerUVount + j);
					rSubMesh.Indices.push_back((i + 1) * ringVerUVount + j + 1);

					rSubMesh.Indices.push_back(i * ringVerUVount + j);
					rSubMesh.Indices.push_back((i + 1) * ringVerUVount + j + 1);
					rSubMesh.Indices.push_back(i * ringVerUVount + j + 1);
				}
			}

			BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, rSubMesh);
			BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, rSubMesh);

			return Mesh;
		}

		void BuildCylinderTopCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::SubMesh& meshData)
		{
			UINT32 baseIndex = (UINT32)meshData.Vertices.size();

			FLOAT y = 0.5f * height;
			FLOAT dTheta = 2.0f * PI / sliceCount;

			// Duplicate cap ring vertices because the texture coordinates and normals differ.
			for (UINT32 i = 0; i <= sliceCount; ++i)
			{
				FLOAT x = topRadius * cosf(i * dTheta);
				FLOAT z = topRadius * sinf(i * dTheta);

				// Scale down by the height to try and make top cap texture coord area
				// proportional to base.
				FLOAT u = x / height + 0.5f;
				FLOAT v = z / height + 0.5f;

				meshData.Vertices.push_back(FVertex({ x, y, z }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			meshData.Vertices.push_back(FVertex({ 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Index of center vertex.
			UINT32 centerIndex = (UINT32)meshData.Vertices.size() - 1;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				meshData.Indices.push_back(centerIndex);
				meshData.Indices.push_back(baseIndex + i + 1);
				meshData.Indices.push_back(baseIndex + i);
			}
		}

		void BuildCylinderBottomCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::SubMesh& meshData)
		{
			// 
			// Build bottom cap.
			//

			UINT32 baseIndex = (UINT32)meshData.Vertices.size();
			FLOAT y = -0.5f * height;

			// vertices of ring
			FLOAT dTheta = 2.0f * PI / sliceCount;
			for (UINT32 i = 0; i <= sliceCount; ++i)
			{
				FLOAT x = bottomRadius * cosf(i * dTheta);
				FLOAT z = bottomRadius * sinf(i * dTheta);

				// Scale down by the height to try and make top cap texture coord area
				// proportional to base.
				FLOAT u = x / height + 0.5f;
				FLOAT v = z / height + 0.5f;

				meshData.Vertices.push_back(FVertex({ x, y, z }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			meshData.Vertices.push_back(FVertex({ 0.0f, y, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Cache the index of center vertex.
			UINT32 centerIndex = (UINT32)meshData.Vertices.size() - 1;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				meshData.Indices.push_back(centerIndex);
				meshData.Indices.push_back(baseIndex + i);
				meshData.Indices.push_back(baseIndex + i + 1);
			}
		}

		FMesh CreateGrid(FLOAT width, FLOAT depth, UINT32 m, UINT32 n)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();

			UINT32 verUVount = m * n;
			UINT32 faceCount = (m - 1) * (n - 1) * 2;

			//
			// Create the vertices.
			//

			FLOAT halfWidth = 0.5f * width;
			FLOAT halfDepth = 0.5f * depth;

			FLOAT dx = width / (n - 1);
			FLOAT dz = depth / (m - 1);

			FLOAT du = 1.0f / (n - 1);
			FLOAT dv = 1.0f / (m - 1);

			rSubMesh.Vertices.resize(verUVount);
			for (UINT32 i = 0; i < m; ++i)
			{
				FLOAT z = halfDepth - i * dz;
				for (UINT32 j = 0; j < n; ++j)
				{
					FLOAT x = -halfWidth + j * dx;

					rSubMesh.Vertices[i * n + j].Position = FVector3F(x, 0.0f, z);
					rSubMesh.Vertices[i * n + j].Normal = FVector3F(0.0f, 1.0f, 0.0f);
					rSubMesh.Vertices[i * n + j].Tangent = FVector4F(1.0f, 0.0f, 0.0f, 1.0f);

					// Stretch texture over grid.
					rSubMesh.Vertices[i * n + j].UV.x = j * du;
					rSubMesh.Vertices[i * n + j].UV.y = i * dv;
				}
			}

			//
			// Create the indices.
			//

			rSubMesh.Indices.resize(faceCount * 3); // 3 indices per face

			// Iterate over each quad and compute indices.
			UINT32 k = 0;
			for (UINT32 i = 0; i < m - 1; ++i)
			{
				for (UINT32 j = 0; j < n - 1; ++j)
				{
					rSubMesh.Indices[k] = i * n + j;
					rSubMesh.Indices[k + 1] = i * n + j + 1;
					rSubMesh.Indices[k + 2] = (i + 1) * n + j;

					rSubMesh.Indices[k + 3] = (i + 1) * n + j;
					rSubMesh.Indices[k + 4] = i * n + j + 1;
					rSubMesh.Indices[k + 5] = (i + 1) * n + j + 1;

					k += 6; // next quad
				}
			}

			return Mesh;
		}

		FMesh CreateQuad(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT depth)
		{
			FMesh Mesh;
			auto& rSubMesh = Mesh.SubMeshes.emplace_back();


			rSubMesh.Vertices.resize(4);
			rSubMesh.Indices.resize(6);

			// Position coordinates specified in NDC space.
			rSubMesh.Vertices[0] = FVertex(
				{ x, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 1.0f }
			);

			rSubMesh.Vertices[1] = FVertex(
				{ x, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 0.0f }
			);

			rSubMesh.Vertices[2] = FVertex(
				{ x + w, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 0.0f }
			);

			rSubMesh.Vertices[3] = FVertex(
				{ x + w, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 1.0f }
			);

			rSubMesh.Indices[0] = 0;
			rSubMesh.Indices[1] = 1;
			rSubMesh.Indices[2] = 2;

			rSubMesh.Indices[3] = 0;
			rSubMesh.Indices[4] = 2;
			rSubMesh.Indices[5] = 3;

			return Mesh;
		}

	}

}
