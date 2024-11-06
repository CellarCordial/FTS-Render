#ifndef MATH_BOUNDS_H
#define MATH_BOUNDS_H

#include "Vector.h"
#include "Ray.h"
#include <vector>

namespace FTS 
{
    template <typename T> requires std::is_arithmetic_v<T> struct TBounds2;
    template <typename T> requires std::is_arithmetic_v<T> struct TBounds3;

	template <typename T>
	requires std::is_arithmetic_v<T>
	struct TBounds2
	{
		TBounds2()
		{
			T Max = std::numeric_limits<T>::max();
			T Min = std::numeric_limits<T>::lowest();

			m_Min = TVector2<T>(Max, Max);
			m_Max = TVector2<T>(Min, Min);
		}

		explicit TBounds2(const TVector2<T>& crPoint) : m_Min(crPoint), m_Max(crPoint) {}
		
		TBounds2(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2)
		{
			m_Max = Max(crPoint1, crPoint2);
			m_Min = Min(crPoint1, crPoint2);
		}

		const TVector2<T>& operator[](UINT32 dwIndex) const
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return m_Min;
			return m_Max;
		}

		TVector2<T>& operator[](UINT32 dwIndex)
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return m_Min;
			return m_Max;
		}

		BOOL operator==(const TBounds2<T>& crOther)
		{
			return m_Max == crOther.m_Max && m_Min == crOther.m_Min;
		}
		
		BOOL operator!=(const TBounds2<T>& crOther)
		{
			return m_Max != crOther.m_Max || m_Min != crOther.m_Min;
		}

		template <typename U>
		explicit operator TBounds2<U>() const
		{
			return TBounds2<U>(TVector2<U>(m_Min), TVector2<U>(m_Max));
		}

		// 返回沿框的对角线从最小点指向最大点的向量
		TVector2<T> Diagonal() const { return m_Max - m_Min; }

		T Width() const
		{
			return m_Max.x - m_Min.x;
		}

		T Height() const
		{
			return m_Max.y - m_Min.y;
		}

		T Area() const
		{
			TVector2<T> Vec = Diagonal();
			return Vec.x * Vec.y;
		}

		// 返回三轴中最长轴的索引
		UINT32 MaxExtent() const
		{
			TVector2<T> Vec = Diagonal();
			if (Vec.x > Vec.y) return 0;
			return 1;
		}

		// 每个维度的给定量在框的两个顶点之间线性插值，获取一个指向包围盒内部的向量
		TVector2<T> Lerp(const TVector2<T>& crPoint)
		{
			return TVector2<T>(
				Lerp(crPoint.x, m_Min.x, m_Max.x),
				Lerp(crPoint.y, m_Min.y, m_Max.y)
			);
		}

		// 获取点相对包围盒最小顶点的偏移，并归一化
		TVector2<T> Offset(const TVector2<T>& crPoint)
		{
			TVector2<T> o = crPoint - m_Min;
			if (m_Max.x > m_Min.x) o.x /= m_Max.x - m_Min.x;
			if (m_Max.y > m_Min.y) o.y /= m_Max.y - m_Min.y;
			return o;
		}

		void BoundingSphere(TVector2<T>* pCenter, FLOAT* pfRadius)
		{
			*pCenter = (m_Min + m_Max) / 2;
			*pfRadius = Inside(*pCenter, *this) ? Distance(*pCenter, m_Max) : 0;
		}
		
		TVector2<T> m_Min, m_Max;
	};

	using FBounds2F = TBounds2<FLOAT>;
	using FBounds2I = TBounds2<INT32>;

	using FBounds3F = TBounds3<FLOAT>;
	using FBounds3I = TBounds3<INT32>;
	
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct TBounds3
	{
		TBounds3()
		{
			T Max = std::numeric_limits<T>::max();
			T Min = std::numeric_limits<T>::lowest();

			m_Min = TVector3<T>(Max, Max, Max);
			m_Max = TVector3<T>(Min, Min, Min);
		}

		TBounds3(T MinX, T MinY, T MinZ, T MaxX, T MaxY, T MaxZ) :
			m_Min(MinX, MinY, MinZ),
			m_Max(MaxX, MaxY, MaxZ)
		{
		}

		explicit TBounds3(const TVector3<T>& crPoint) : m_Min(crPoint), m_Max(crPoint) {}
		

		/**
		 * @brief       构造函数, 无需在意两者最大最小点之分
		 * 
		 * @param       crPoint1  
		 * @param       crPoint2 
		 */
		TBounds3(const TVector3<T>& crPoint1, const TVector3<T>& crPoint2)
		{
			m_Max = Max(crPoint1, crPoint2);
			m_Min = Min(crPoint1, crPoint2);
		}

		const TVector3<T>& operator[](UINT32 dwIndex) const
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return m_Min;
			return m_Max;
		}

		TVector3<T>& operator[](UINT32 dwIndex)
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return m_Min;
			return m_Max;
		}

		BOOL operator==(const TBounds3<T>& crOther) const
		{
			return m_Max == crOther.m_Max && m_Min == crOther.m_Min;
		}
		
		BOOL operator!=(const TBounds3<T>& crOther) const
		{
			return m_Max != crOther.m_Max || m_Min != crOther.m_Min;
		}

		TBounds3<T>& operator=(const TBounds3<T>& crOther)
		{
			m_Min = crOther.m_Min;
			m_Max = crOther.m_Max;
			return *this;
		}

		template <typename U>
		explicit operator TBounds3<U>() const
		{
			return TBounds3<U>(TVector3<U>(m_Min), TVector3<U>(m_Max));
		}

		// 获取包围盒八个顶点之一的坐标，Corner 属于 [0, 7], 分别对应8个顶点
		TVector3<T> Corner(UINT32 dwCorner) const
		{
			return TVector3<T>(
				(*this)[dwCorner & 1].x,
				(*this)[dwCorner & 2 ? 1 : 0].y,
				(*this)[dwCorner & 4 ? 1 : 0].z
			);
		}

		// 返回沿框的对角线从最小点指向最大点的向量
		TVector3<T> Diagonal() const { return m_Max - m_Min; }

		T SurfaceArea() const
		{
			TVector3<T> Vec = Diagonal();
			return 2 * (Vec.x * Vec.y + Vec.x * Vec.z + Vec.y * Vec.z);
		}

		T Volume() const
		{
			TVector3<T> Vec = Diagonal();
			return Vec.x * Vec.y * Vec.z;
		}

		// 返回三轴中最长轴的索引
		UINT32 MaxExtent() const
		{
			TVector3<T> Vec = Diagonal();
			if (Vec.x > Vec.y && Vec.x > Vec.z) return 0;
			if (Vec.y > Vec.z) return 1;
			return 2;
		}

		FVector3F Extent() const
		{
			return m_Max - m_Min;
		}

		// 每个维度的给定量在框的两个顶点之间线性插值，获取一个指向包围盒内部的向量
		TVector3<T> Lerp(const TVector3<T>& crPoint) const
		{
			return TVector3<T>(
				Lerp(crPoint.x, m_Min.x, m_Max.x),
				Lerp(crPoint.y, m_Min.y, m_Max.y),
				Lerp(crPoint.z, m_Min.z, m_Max.z)
			);
		}

		// 获取点相对包围盒最小顶点的偏移，并归一化
		TVector3<T> Offset(const TVector3<T>& crPoint) const
		{
			TVector3<T> o = crPoint - m_Min;
			if (m_Max.x > m_Min.x) o.x /= m_Max.x - m_Min.x;
			if (m_Max.y > m_Min.y) o.y /= m_Max.y - m_Min.y;
			if (m_Max.z > m_Min.z) o.z /= m_Max.z - m_Min.z;
			return o;
		}

		void BoundingSphere(TVector3<T>* pCenter, FLOAT* pfRadius) const
		{
			*pCenter = (m_Min + m_Max) / 2;
			*pfRadius = Inside(*pCenter, *this) ? Distance(*pCenter, m_Max) : 0;
		}

		
		BOOL IntersectP(const FRay& crRay, FLOAT* pfHit0, FLOAT* pfHit1) const
		{
			// 平面方程：已知空间中一个点和该点所在平面的法向量，即可确定该平面
			// 设平面上一点 $M_0(x_0, y_0, z_0)$, 另一点为 $M(x, y, z)$, 平面法线为 $\vec {n} = (a, b, c)$
			// 则有 $\vec {M_0 M} \cdot \vec {n} = 0$
			// 即为 $a(x - x_0) + b(y - y_0) + c(z - z_0) = 0$, 同 ax + by + cz + m_Dir = 0

			// 将光线方程代入 t 作为平面交点，则变为 $a(o_x + td_x) + b(o_y + td_y) + c(o_z + td_z) = 0$
			// 其中 $-d_{x/y/z}$ 为包围盒最大/小点
			// 化简得 $t = \frac {-d - (a, b, c) \cdot \vec {o}}{(a, b, c) \cdot \vec {d}}$
			// 以垂直于 x 轴的法线为例，其 y z 分量都为 0, 则上式可以化简为 $t = \frac {-d - o_x}{d_x}$


			FLOAT fT0 = 0.0f, fT1 = crRay.m_fMax;	// 与包围盒的两个相交点

			// 对包围盒每对平面求交点
			for (UINT32 ix = 0; ix < 3; ++ix)
			{
				// 根据公式 $t = \frac {-d - o_x}{d_x}$, 先求 $d_{x/y/z} $ 的倒数

				// 若 crRay.m_Dir[ix], 即光线 crRay 与正在求交的的平面平行或在平面内，则 fInvDir 会变为 INF, 这样也能正确工作
				FLOAT fInvDir = 1.0f / crRay.m_Dir[ix];
				FLOAT fTNear = (m_Min[ix] - crRay.m_Ori[ix]) * fInvDir;
				FLOAT fTFar = (m_Max[ix] - crRay.m_Ori[ix]) * fInvDir;

				if (fTNear > fTFar) std::swap(fTNear, fTFar);

				// 保证交点计算的精度
				// 考虑浮点误差的计算 a = (b - c) * (1 / d) 中有三个运算符 
				// 故 n 应为 3, 使用 Gamma(3)
				// 至于为什么是两倍的 Gamma(3), 这需要考虑 $t_{min}$ $t_{max}$ 的重叠问题
				// 当 $t_{max}$ 的误差区间和 $t_{min}$ 的误差区间交叠，则比较 $t_{min} < t_{max}$ 可能不能实际反映出光线是否命中边界框
				// 这种情况下保守地返回真比错过确有的相交更好，用tmax 误差的两倍对其扩展保证比较是保守的
				// 如下图
				//			$t_{max}$
				//    |———————————·———————————|
				// ———————————————·———·——————————————————
				//				|—————·—————|
				//				 $t_{min}$
				fTFar *= 1 + 2 * Gamma(3);
				
				// 迭代比较
				fT0 = fTNear > fT0 ? fTNear : fT0;
				fT1 = fTFar < fT1 ? fTFar : fT1;

				// 无交点
				if (fT0 > fT1) return false;
			}

			if (pfHit0 != nullptr) *pfHit0 = fT0;
			if (pfHit1 != nullptr) *pfHit1 = fT1;

			return true;
		}

		// 检查光线 crRay 是否与该包围盒相交，与上面的 IntersectP() 方法相比，该方法能带来很大的性能提升
		// crRayInvDir: 预计算的光线方向三个分量的倒数
		// dwDirIsNeg: 光线三个分量是否是原本方向的负，0 代表与原本方向一致，1 代表与原本方向相反
		BOOL IntersectP(const FRay& crRay, const FVector3F& crRayInvDir, const UINT32 dwDirIsNeg[3]) const
		{
			const FBounds3F& crBounds = *this;

			// 公式 $t = \frac {-d - o_x}{d_x}$

			const FLOAT fTMinX = (crBounds[dwDirIsNeg[0]].x - crRay.m_Ori.x) / crRayInvDir.x;
			FLOAT fTMaxX = (crBounds[1 - dwDirIsNeg[0]].x - crRay.m_Ori.x) / crRayInvDir.x;
			
			const FLOAT fTMinY = (crBounds[dwDirIsNeg[1]].y - crRay.m_Ori.y) / crRayInvDir.y;
			FLOAT fTMaxY = (crBounds[1 - dwDirIsNeg[1]].y - crRay.m_Ori.y) / crRayInvDir.y;

			// 看 1610 行
			fTMaxX *= 1 + 2 * Gamma(3);
			fTMaxY *= 1 + 2 * Gamma(3);

			FLOAT fT0 = fTMinX, fT1 = fTMaxX;	// 与包围盒的两个相交点

			// 三个分量迭代比较转变为取三个中最小和最大的，但小的不能比大的大，反过来同理
			if (fT0 > fTMaxY || fT1 < fTMinY) return false;
			if (fT0 < fTMinY) fT0 = fTMinY;
			if (fT1 > fTMaxY) fT1 = fTMaxY;

			// 同上
			const FLOAT fTMinZ = (crBounds[dwDirIsNeg[2]].z - crRay.m_Ori.z) / crRayInvDir.z;
			FLOAT fTMaxZ = (crBounds[1 - dwDirIsNeg[2]].z - crRay.m_Ori.z) / crRayInvDir.z;

			// 看 1610 行
			fTMaxZ *= 1 + 2 * Gamma(3);

			if (fT0 > fTMaxZ || fT1 < fTMinZ) return false;
			if (fT0 < fTMinZ) fT0 = fTMinZ;
			if (fT1 > fTMaxZ) fT1 = fTMaxZ;

			// 确保合理
			return fT0 < crRay.m_fMax && fT1 > 0;
		}

		TVector3<T> m_Min, m_Max;
	};

    
	// 组合包围盒和点
	template <typename T>
	TBounds3<T> Union(const TBounds3<T>& crBounds, const TVector3<T>& crPoint)
	{
		return TBounds3<T>(Min(crBounds.m_Min, crPoint), Max(crBounds.m_Max, crPoint));
	}

	// 组合两个包围盒
	template <typename T>
	TBounds3<T> Union(const TBounds3<T>& crBounds1, const TBounds3<T>& crBounds2)
	{
		return TBounds3<T>(Min(crBounds1.m_Min, crBounds2.m_Min), Max(crBounds1.m_Max, crBounds2.m_Max));
	}

	// 组合包围盒和点
	template <typename T>
	TBounds2<T> Union(const TBounds2<T>& crBounds, const TVector2<T>& crPoint)
	{
		return TBounds2<T>(Min(crBounds.m_Min, crPoint), Max(crBounds.m_Max, crPoint));
	}

	// 组合两个包围盒
	template <typename T>
	TBounds2<T> Union(const TBounds2<T>& crBounds1, const TBounds2<T>& crBounds2)
	{
		return TBounds2<T>(Min(crBounds1.m_Min, crBounds2.m_Min), Max(crBounds1.m_Max, crBounds2.m_Max));
	}

	// 获取两个包围盒相交处的包围盒
	template <typename T>
	TBounds3<T> Intersect(const TBounds3<T>& crBounds1, const TBounds3<T>& crBounds2)
	{
		return TBounds3<T>(Max(crBounds1.m_Min, crBounds2.m_Min), Min(crBounds1.m_Max, crBounds2.m_Max));
	}

	// 获取两个包围盒相交处的包围盒
	template <typename T>
	TBounds2<T> Intersect(const TBounds2<T>& crBounds1, const TBounds2<T>& crBounds2)
	{
		return TBounds2<T>(Max(crBounds1.m_Min, crBounds2.m_Min), Min(crBounds1.m_Max, crBounds2.m_Max));
	}

	// 判断是否相交/重叠
	template <typename T>
	BOOL Overlaps(const TBounds3<T>& crBounds1, const TBounds3<T>& crBounds2)
	{
		const BOOL x = crBounds1.m_Max.x > crBounds2.m_Min.x && crBounds1.m_Min.x < crBounds2.m_Max.x;
		const BOOL y = crBounds1.m_Max.y > crBounds2.m_Min.y && crBounds1.m_Min.y < crBounds2.m_Max.y;
		const BOOL z = crBounds1.m_Max.z > crBounds2.m_Min.z && crBounds1.m_Min.x < crBounds2.m_Max.z;

		return x && y && z;
	}

	// 判断是否相交/重叠
	template <typename T>
	BOOL Overlaps(const TBounds2<T>& crBounds1, const TBounds2<T>& crBounds2)
	{
		const BOOL x = crBounds1.m_Max.x > crBounds2.m_Min.x && crBounds1.m_Min.x < crBounds2.m_Max.x;
		const BOOL y = crBounds1.m_Max.y > crBounds2.m_Min.y && crBounds1.m_Min.y < crBounds2.m_Max.y;
		return x && y;
	}

	// 判断点是否在包围盒内
	template <typename T>
	BOOL Inside(const TVector3<T>& crPoint, const TBounds3<T>& crBound)
	{
		return	crPoint.x >= crBound.m_Min.x && crPoint.x <= crBound.m_mMax.x &&
				crPoint.y >= crBound.m_Min.y && crPoint.y <= crBound.m_mMax.y &&
				crPoint.z >= crBound.m_Min.z && crPoint.z <= crBound.m_mMax.z;
	}

	// 判断点是否在包围盒内
	template <typename T>
	BOOL Inside(const TVector2<T>& crPoint, const TBounds2<T>& crBound)
	{
		return	crPoint.x >= crBound.m_Min.x && crPoint.x <= crBound.m_mMax.x &&
				crPoint.y >= crBound.m_Min.y && crPoint.y <= crBound.m_mMax.y;
	}
	
	// 判断点是否在包围盒内，不包含上边界
	template <typename T>
	BOOL InsideExclusive(const TVector3<T>& crPoint, const TBounds3<T>& crBound)
	{
		return	crPoint.x >= crBound.m_Min.x && crPoint.x < crBound.m_mMax.x &&
				crPoint.y >= crBound.m_Min.y && crPoint.y < crBound.m_mMax.y &&
				crPoint.z >= crBound.m_Min.z && crPoint.z < crBound.m_mMax.z;
	}

	// 判断点是否在包围盒内，不包含上边界
	template <typename T>
	BOOL InsideExclusive(const TVector2<T>& crPoint, const TBounds2<T>& crBound)
	{
		return	crPoint.x >= crBound.m_Min.x && crPoint.x < crBound.m_mMax.x &&
				crPoint.y >= crBound.m_Min.y && crPoint.y < crBound.m_mMax.y;
	}

	// 扩充包围盒边界框
	template <typename T, typename U>
	TBounds3<T> Expand(const TBounds3<T>& crBound, U Delta)
	{
		return TBounds3<T>(
			crBound.m_Min - TVector3<U>(Delta),
			crBound.m_Max + TVector3<U>(Delta)
		);
	}

	// 扩充包围盒边界框
	template <typename T, typename U>
	TBounds2<T> Expand(const TBounds2<T>& crBound, U Delta)
	{
		return TBounds2<T>(
			crBound.m_Min - TVector2<U>(Delta),
			crBound.m_Max + TVector2<U>(Delta)
		);
	}

    FBounds3F CreateAABB(const std::vector<FVector3F>& InPosition);

}







#endif