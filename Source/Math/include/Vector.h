#ifndef MATH_CONTAINER_H
#define MATH_CONTAINER_H

#include <type_traits>
#include <cmath>
#include "Common.h"

namespace FTS 
{
    
    // Forward declaration
    template <typename T> requires std::is_arithmetic_v<T> struct TVector2;
    template <typename T> requires std::is_arithmetic_v<T> struct TVector3;
    template <typename T> requires std::is_arithmetic_v<T> struct TVector4;
    
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct TVector2
	{
		TVector2() : x(0), y(0) {}

		TVector2(T _x, T _y) : x(_x), y(_y) {}

		TVector2(const TVector2<T>& crVector) : x(crVector.x), y(crVector.y) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit TVector2(const TVector2<U>& crVector) :
			x(static_cast<T>(crVector.x)),
			y(static_cast<T>(crVector.y))
		{
		}

		TVector2& operator=(const TVector2<T>& crVector)
		{
			x = crVector.x;
			y = crVector.y;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector2& operator=(const TVector2<U>& crVector)
		{
			x = static_cast<T>(crVector.x);
			y = static_cast<T>(crVector.y);
			return *this;
		}

		TVector2<T> operator+(const TVector2<T>& crVector) const { return TVector2(x + crVector.x, y + crVector.y); }

		TVector2<T> operator-(const TVector2<T>& crVector) const { return TVector2(x - crVector.x, y - crVector.y); }

		TVector2<T> operator+(T Value) const
		{
			return TVector2(x + Value, y + Value);
		}

		TVector2<T> operator-(T Value) const
		{
			return TVector2(x - Value, y - Value);
		}

		TVector2<T> operator-() const { return TVector2(-x, -y); }

		TVector2<T>& operator+=(const TVector2<T>& crVector)
		{
			x += crVector.x;
			y += crVector.y;
			return *this; 
		}

		TVector2<T>& operator-=(const TVector2<T>& crVector)
		{
			x -= crVector.x;
			y -= crVector.y;
			return *this;
		}

		BOOL operator==(const TVector2<T>& crVector) const
		{
			return x == crVector.x && y == crVector.y;
		}

		BOOL operator!=(const TVector2<T>& crVector) const
		{
			return x != crVector.x || y != crVector.y;
		}

		BOOL operator>(const TVector2<T>& crVector) const
		{
			return LengthSquared() > crVector.LengthSquared();
		}

		BOOL operator<(const TVector2<T>& crVector) const
		{
			return LengthSquared() < crVector.LengthSquared();
		}

		BOOL operator<=>(const TVector2<T>& crVector)
		{
			return LengthSquared() <=> crVector.LengthSquared();
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector2<T> operator*(U _u) const { return TVector2(x * _u, y * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector2<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			return *this; 
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector2<T> operator/(U _u) const
		{
			// FCHECK_NE(_u, 0);
			FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			return TVector2(x * fInv, y * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector2<T>& operator/=(U _u)
		{
			// FCHECK_NE(_u, 0);
			FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			x *= fInv;
			y *= fInv;
			return *this;
		}

		T operator[](UINT32 dwIndex) const
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return x;
			return y;
		}

		T& operator[](UINT32 dwIndex)
		{
			// FCHECK(dwIndex <= 1);
			if (dwIndex == 0) return x;
			return y;
		}

		T LengthSquared() const { return x * x + y * y; }

		FLOAT Length() const { return std::sqrt(LengthSquared()); }

		T x, y;
	};
	
	using FVector2F = TVector2<FLOAT>;
	using FVector2I = TVector2<UINT32>;

	template <typename T>
	requires std::is_arithmetic_v<T>
	struct TVector3
	{
		TVector3() : x(0), y(0), z(0) {}

		explicit TVector3(T _Num) : x(_Num), y(_Num), z(_Num) {}

		TVector3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}

		explicit TVector3(const TVector4<T>& crVector) : x(crVector.x), y(crVector.y), z(crVector.z) {}

		TVector3(const TVector3<T>& crVector) : x(crVector.x), y(crVector.y), z(crVector.z) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit TVector3(const TVector3<U>& crVector) :
			x(static_cast<T>(crVector.x)),
			y(static_cast<T>(crVector.y)),
			z(static_cast<T>(crVector.z))
		{
		}

		TVector3& operator=(const TVector3<T>& crVector)
		{
			x = crVector.x;
			y = crVector.y;
			z = crVector.z;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector3& operator=(const TVector3<U>& crVector)
		{
			x = static_cast<T>(crVector.x);
			y = static_cast<T>(crVector.y);
			z = static_cast<T>(crVector.z);
			return *this;
		}

		TVector3<T> operator+(const TVector3<T>& crVector) const { return TVector3(x + crVector.x, y + crVector.y, z + crVector.z); }

		TVector3<T> operator-(const TVector3<T>& crVector) const { return TVector3(x - crVector.x, y - crVector.y, z - crVector.z); }

		TVector3<T> operator+(T Value) const
		{
			return TVector3(x + Value, y + Value, z + Value);
		}

		TVector3<T> operator-(T Value) const
		{
			return TVector3(x - Value, y - Value, z - Value);
		}

		TVector3<T> operator-() const { return TVector3(-x, -y, -z); }

		TVector3<T>& operator+=(const TVector3<T>& crVector)
		{
			x += crVector.x;
			y += crVector.y;
			z += crVector.z;
			return *this;
		}

		TVector3<T>& operator-=(const TVector3<T>& crVector)
		{
			x -= crVector.x;
			y -= crVector.y;
			z -= crVector.z;
			return *this;
		}

		BOOL operator==(const TVector3<T>& crVector) const
		{
			return x == crVector.x && y == crVector.y && z == crVector.z;
		}

		BOOL operator!=(const TVector3<T>& crVector) const
		{
			return x != crVector.x || y != crVector.y || z != crVector.z;
		}

		BOOL operator>(const TVector3<T>& crVector) const
		{
			return LengthSquared() > crVector.LengthSquared();
		}

		BOOL operator<(const TVector3<T>& crVector) const
		{
			return LengthSquared() < crVector.LengthSquared();
		}

		BOOL operator<=>(const TVector3<T>& crVector) const
		{
			return LengthSquared() <=> crVector.LengthSquared();
		}

		TVector3<T> operator*(const TVector3<T>& crVec) const
		{
			return TVector3(x * crVec.x, y * crVec.y, z * crVec.z); 
		}

		TVector3<T>& operator*=(const TVector3<T>& crVec) const
		{
			return (*this) * crVec;
		}


		template <typename U>
		TVector3<T> operator*(U _u) const { return TVector3(x * _u, y * _u, z * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector3<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			z *= _u;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector3<T> operator/(U _u) const
		{
			// FCHECK_NE(_u, 0);
			const FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			return TVector3(x * fInv, y * fInv, z * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector3<T>& operator/=(U _u)
		{
			// FCHECK_NE(_u, 0);
			const FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			x *= fInv;
			y *= fInv;
			z *= fInv;
			return *this;
		}

		T operator[](UINT32 dwIndex) const
		{
			// FCHECK(dwIndex <= 2);
			if (dwIndex == 0) return x;
			if (dwIndex == 1) return y;
			return z;
		}

		T& operator[](UINT32 dwIndex)
		{
			// FCHECK(dwIndex <= 2);
			if (dwIndex == 0) return x;
			if (dwIndex == 1) return y;
			return z;
		}

		T LengthSquared() const { return x * x + y * y + z * z; }

		FLOAT Length() const { return std::sqrt(LengthSquared()); }

		T x, y, z;
	};

	using FVector3F = TVector3<FLOAT>;
	using FVector3I = TVector3<UINT32>;

	// w 分量初始化为 1
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct TVector4
	{
		TVector4() : x(0), y(0), z(0), w(1) {}

		explicit TVector4(T _Num) : x(_Num), y(_Num), z(_Num), w(_Num) {}

		TVector4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) {}

		TVector4(const TVector4<T>& crVector) : x(crVector.x), y(crVector.y), z(crVector.z), w(crVector.w) {}

		explicit TVector4(const TVector3<T>& crVector, T _w = 1) : x(crVector.x), y(crVector.y), z(crVector.z), w(_w) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit TVector4(const TVector4<U>& crVector) :
			x(static_cast<T>(crVector.x)),
			y(static_cast<T>(crVector.y)),
			z(static_cast<T>(crVector.z)),
			w(static_cast<T>(crVector.w))
		{
		}

		TVector4& operator=(const TVector4<T>& crVector)
		{
			x = crVector.x;
			y = crVector.y;
			z = crVector.z;
			w = crVector.w;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector4& operator=(const TVector4<U>& crVector)
		{
			x = static_cast<T>(crVector.x);
			y = static_cast<T>(crVector.y);
			z = static_cast<T>(crVector.z);
			w = static_cast<T>(crVector.w);
			return *this;
		}

		TVector4<T> operator+(const TVector4<T>& crVector) const
		{
			return TVector4(x + crVector.x, y + crVector.y, z + crVector.z, w + crVector.w);
		}

		TVector4<T> operator-(const TVector4<T>& crVector) const
		{
			return TVector4(x - crVector.x, y - crVector.y, z - crVector.z, w - crVector.w);
		}

		TVector4<T> operator+(T Value) const
		{
			return TVector4(x + Value, y + Value, z + Value, w + Value);
		}

		TVector4<T> operator-(T Value) const
		{
			return TVector4(x - Value, y - Value, z - Value, w - Value);
		}

		TVector4<T> operator-() const { return TVector4(-x, -y, -z, -w); }

		TVector4<T>& operator+=(const TVector4<T>& crVector)
		{
			x += crVector.x;
			y += crVector.y;
			z += crVector.z;
			w += crVector.w;
			return *this;
		}

		TVector4<T>& operator-=(const TVector4<T>& crVector)
		{
			x -= crVector.x;
			y -= crVector.y;
			z -= crVector.z;
			w -= crVector.w;
			return *this;
		}

		BOOL operator==(const TVector4<T>& crVector) const
		{
			return x == crVector.x && y == crVector.y && z == crVector.z && w == crVector.w;
		}

		BOOL operator!=(const TVector4<T>& crVector) const
		{
			return x != crVector.x || y != crVector.y || z != crVector.z ||  w != crVector.w;
		}

		BOOL operator>(const TVector4<T>& crVector) const
		{
			return LengthSquared() > crVector.LengthSquared();
		}

		BOOL operator<(const TVector4<T>& crVector) const
		{
			return LengthSquared() < crVector.LengthSquared();
		}

		BOOL operator<=>(const TVector4<T>& crVector) const
		{
			return LengthSquared() <=> crVector.LengthSquared();
		}

		template <typename U>
		TVector4<T> operator*(U _u) const { return TVector4(x * _u, y * _u, z * _u, w * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector4<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			z *= _u;
			w *= _u;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector4<T> operator/(U _u) const
		{
			// FCHECK_NE(_u, 0);
			const FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			return TVector4(x * fInv, y * fInv, z * fInv, w * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		TVector4<T>& operator/=(U _u)
		{
			// FCHECK_NE(_u, 0);
			const FLOAT fInv = 1 / static_cast<FLOAT>(_u);
			x *= fInv;
			y *= fInv;
			z *= fInv;
			w *= fInv;
			return *this;
		}

		T operator[](UINT32 dwIndex) const
		{
			// FCHECK(dwIndex <= 3);
			if (dwIndex == 0) return x;
			if (dwIndex == 1) return y;
			if (dwIndex == 2) return z;
			return w;
		}

		T& operator[](UINT32 dwIndex)
		{
			// FCHECK(dwIndex <= 2);
			if (dwIndex == 0) return x;
			if (dwIndex == 1) return y;
			if (dwIndex == 2) return z;
			return w;
		}

		T LengthSquared() const { return x * x + y * y + z * z + w * w; }

		FLOAT Length() const { return std::sqrt(LengthSquared()); }

		T x, y, z, w;
	};

	using FVector4F = TVector4<FLOAT>;
	using FVector4I = TVector4<INT32>;

    
	template <typename U, typename T>
	inline TVector4<T> operator*(U _u, const TVector4<T>& crVec)
	{
		return crVec * _u;
	}
	
	template <typename U, typename T>
	inline TVector3<T> operator*(U _u, const TVector3<T>& crVec)
	{
		return crVec * _u;
	}
	
	template <typename U, typename T>
	inline TVector2<T> operator*(U _u, const TVector2<T>& crVec)
	{
		return crVec * _u;
	}
	
	template <typename T>
	inline T Dot(const TVector3<T>& crVec1, const TVector3<T>& crVec2)
	{
		return crVec1.x * crVec2.x + crVec1.y * crVec2.y + crVec1.z * crVec2.z;
	}
	
	template <typename T>
	inline T Dot(const TVector2<T>& crVec1, const TVector2<T>& crVec2)
	{
		return crVec1.x * crVec2.x + crVec1.y * crVec2.y;
	}

	template <typename T>
	inline T AbsDot(const TVector3<T>& crVec1, const TVector3<T>& crVec2)
	{
		return std::abs(Dot(crVec1, crVec2));
	}

	template <typename T>
	inline T AbsDot(const TVector2<T>& crVec1, const TVector2<T>& crVec2)
	{
		return std::abs(Dot(crVec1, crVec2));
	}

	template <typename T>
	inline TVector3<T> Cross(const TVector3<T>& crVec1, const TVector3<T>& crVec2)
	{
		// 使用 DOUBLE 可以防止两个值非常接近的浮点数相减造成的误差
		const DOUBLE dVec1X = crVec1.x, dVec1Y = crVec1.y, dVec1Z = crVec1.z;
		const DOUBLE dVec2X = crVec2.x, dVec2Y = crVec2.y, dVec2Z = crVec2.z;
		return TVector3<T>{
			static_cast<T>(dVec1Y * dVec2Z - dVec1Z * dVec2Y),
			static_cast<T>(dVec1Z * dVec2X - dVec1X * dVec2Z),
			static_cast<T>(dVec1X * dVec2Y - dVec1Y * dVec2X)
		};
	}
	
	template <typename T>
	inline TVector4<T> Normalize(const TVector4<T>& crVec)
	{
		return crVec / crVec.Length();
	}

	template <typename T>
	inline TVector3<T> Normalize(const TVector3<T>& crVec)
	{
		return crVec / crVec.Length();
	}
	
	template <typename T>
	inline TVector2<T> Normalize(const TVector2<T>& crVec)
	{
		return crVec / crVec.Length();
	}
	
	// 最小坐标值
	template <typename T>
	inline T MinComponent(const TVector3<T>& crVec)
	{
		return (std::min)(crVec.x, (std::min)(crVec.y, crVec.z));
	}

	// 最大坐标值
	template <typename T>
	inline T MaxComponent(const TVector3<T>& crVec)
	{
		return (std::max)(crVec.x, (std::max)(crVec.y, crVec.z));
	}

	// 最大坐标值索引，即投影最长的维度
	template <typename T>
	inline UINT32 MaxDimension(const TVector3<T>& crVec)
	{
		return (crVec.x > crVec.y) ? (crVec.x > crVec.z ? 0 : 2) : (crVec.y > crVec.z ? 1 : 2);
	}

	// 最小坐标值索引，即投影最短的维度
	template <typename T>
	inline UINT32 MinDimension(const TVector3<T>& crVec)
	{
		return (crVec.x < crVec.y) ? (crVec.x < crVec.z ? 0 : 2) : (crVec.y < crVec.z ? 1 : 2);
	}

	// 取两向量各坐标最小值
	template <typename T>
	inline TVector3<T> Min(const TVector3<T>& crVec1, const TVector3<T>& crVec2)
	{
		return TVector3<T>{
			(std::min)(crVec1.x, crVec2.x),
			(std::min)(crVec1.y, crVec2.y),
			(std::min)(crVec1.z, crVec2.z)
		};
	}

	// 取两向量各坐标最大值
	template <typename T>
	inline TVector3<T> Max(const TVector3<T>& crVec1, const TVector3<T>& crVec2)
	{
		return TVector3<T>{
			(std::max)(crVec1.x, crVec2.x),
			(std::max)(crVec1.y, crVec2.y),
			(std::max)(crVec1.z, crVec2.z)
		};
	}

	// 重新排列坐标
	template <typename T>
	inline TVector3<T> Permute(const TVector3<T>& crVec, UINT32 _x, UINT32 _y, UINT32 _z)
	{
		return TVector3<T>(crVec[_x], crVec[_y], crVec[_z]);
	}

	// 获取单个向量所组成的坐标系基底
	template <typename T>
	inline void CoordinateSystem(const TVector3<T>& crVec1, TVector3<T>* crVec2, TVector3<T>* crVec3)
	{
		auto Vec1 = Normalize(crVec1);
		if (std::abs(Vec1.x) > std::abs(Vec1.y))
		{
			*crVec2 = TVector3(-Vec1.z, 0, Vec1.x) / 
				std::sqrt(Vec1.x * Vec1.x + Vec1.z * Vec1.z);
		}
		else
		{
			*crVec2 = TVector3(0, Vec1.z, -Vec1.y) /
				std::sqrt(Vec1.z * Vec1.z + Vec1.y * Vec1.y);
		}

		*crVec3 = Cross(crVec1, *crVec2);
	}

	// 两点距离
	template <typename T>
	inline FLOAT Distance(const TVector3<T>& crPoint1, const TVector3<T>& crPoint2)
	{
		return (crPoint1 - crPoint2).Length();
	}

	// 两点距离
	template <typename T>
	inline FLOAT Distance(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2)
	{
		return (crPoint1 - crPoint2).Length();
	}

	// 两点距离平方
	template <typename T>
	inline FLOAT DistanceSquared(const TVector3<T>& crPoint1, const TVector3<T>& crPoint2)
	{
		return (crPoint1 - crPoint2).LengthSquared();
	}

	// 两点距离平方
	template <typename T>
	inline FLOAT DistanceSquared(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2)
	{
		return (crPoint1 - crPoint2).LengthSquared();
	}

	// 点的插值
	template <typename T>
	inline TVector3<T> Lerp(const TVector3<T>& crPoint1, const TVector3<T>& crPoint2, FLOAT f)
	{
		FLOAT fLerp = (std::min)(1.0f, (std::max)(0.0f, f));
		return (1 - fLerp) * crPoint1 + fLerp * crPoint2;
	}

	// 点的插值
	template <typename T>
	inline TVector2<T> Lerp(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2, FLOAT f)
	{
		return (1 - f) * crPoint1 + f * crPoint2;
	}

	// 向量的插值
	template <typename T>
	inline TVector4<T> Lerp(FLOAT f, const TVector4<T>& crVec1, const TVector4<T>& crVec2)
	{
		FLOAT fLerp = (std::min)(1.0f, (std::max)(0.0f, f));
		return TVector4<T>((1 - fLerp) * crVec1 + fLerp * crVec2);
	}

	// 取两点各坐标最小值
	template <typename T>
	inline TVector2<T> Min(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2)
	{
		return TVector2<T>{
			(std::min)(crPoint1.x, crPoint2.x),
			(std::min)(crPoint1.y, crPoint2.y)
		};
	}

	// 取两点各坐标最大值
	template <typename T>
	inline TVector2<T> Max(const TVector2<T>& crPoint1, const TVector2<T>& crPoint2)
	{
		return TVector2<T>{
			(std::max)(crPoint1.x, crPoint2.x),
			(std::max)(crPoint1.y, crPoint2.y),
			(std::max)(crPoint1.z, crPoint2.z)
		};
	}

	template <typename T>
	inline TVector3<T> Floor(const TVector3<T>& crPoint)
	{
		return TVector3<T>{std::floor(crPoint.x), std::floor(crPoint.y), std::floor(crPoint.z)};
	}

	template <typename T>
	inline TVector2<T> Floor(const TVector2<T>& crPoint)
	{
		return TVector2<T>{std::floor(crPoint.x), std::floor(crPoint.y)};
	}

	template <typename T>
	inline TVector3<T> Ceil(const TVector3<T>& crPoint)
	{
		return TVector3<T>{std::ceil(crPoint.x), std::ceil(crPoint.y), std::ceil(crPoint.z)};
	}

	template <typename T>
	inline TVector2<T> Ceil(const TVector2<T>& crPoint)
	{
		return TVector2<T>{std::ceil(crPoint.x), std::ceil(crPoint.y)};
	}
	
	template <typename T>
	inline TVector3<T> Abs(const TVector3<T>& crPoint)
	{
		return TVector3<T>{std::abs(crPoint.x), std::abs(crPoint.y), std::abs(crPoint.z)};
	}

	// 调整第一个向量参数的方向，使之与第二个向量参数的方向一致
	template <typename T>
	inline TVector3<T> Faceforward(const TVector3<T> &v1, const TVector3<T> &v2)
	{
		return Dot(v1, v2) < 0.0f ? -v1 : v1;
	}

    	// 给定单位球面坐标系中点的仰角和方位角，使用默认基向量，返回对应的三位向量
	inline FVector3F SphericalDirection(FLOAT fSinTheta, FLOAT fCosTheta, FLOAT fPhi)
	{
		return FVector3F(fSinTheta * std::cos(fPhi), fSinTheta * std::sin(fPhi), fCosTheta);
	}

	// 给定单位球面坐标系中点的仰角和方位角，以及基向量，返回对应的三位向量
	inline FVector3F SphericalDirection(
		FLOAT fSinTheta, 
		FLOAT fCosTheta, 
		FLOAT fPhi, 
		const FVector3F& x, 
		const FVector3F& y, 
		const FVector3F& z
	)
	{
		// z = r * cos(theta)
        // x = r * sin(theta) * cos(phi)
        // y = r * sin(theta) * sin(phi)
		// 单位球面，r = 1，于是分别乘以相应的基向量
		return fSinTheta * std::cos(fPhi) * x + fSinTheta * std::sin(fPhi) * y + fCosTheta * z;
	}

	// 计算向量 crVec 与 z 轴的夹角（仰角），即球面坐标系中的 theta 角
	inline FLOAT SphericalTheta(const FVector3F& crVec)
	{
		return std::acos(Clamp(crVec.z, -1, 1));
	}

	// 计算向量 crVec 在 xy 平面上的投影的方位角，即球面坐标系中的 phi 角
	inline FLOAT SphericalPhi(const FVector3F& crVec)
	{
		// std::atan2 的值域为 [-π，π]
		FLOAT p = std::atan2(crVec.y, crVec.x);
		return p < 0.0f ? p + 2 * PI : p;
	}

}















#endif