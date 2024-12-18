#ifndef MATH_CONTAINER_H
#define MATH_CONTAINER_H

#include <type_traits>
#include <cmath>
#include "common.h"
#include <algorithm>

namespace fantasy 
{
    
    // Forward declaration
    template <typename T> requires std::is_arithmetic_v<T> struct Vector2;
    template <typename T> requires std::is_arithmetic_v<T> struct Vector3;
    template <typename T> requires std::is_arithmetic_v<T> struct Vector4;
    
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Vector2
	{
		Vector2() : x(0), y(0) {}

		Vector2(T _x, T _y) : x(_x), y(_y) {}

		Vector2(const Vector2<T>& vec) : x(vec.x), y(vec.y) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit Vector2(const Vector2<U>& vec) :
			x(static_cast<T>(vec.x)),
			y(static_cast<T>(vec.y))
		{
		}

		Vector2& operator=(const Vector2<T>& vec)
		{
			x = vec.x;
			y = vec.y;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector2& operator=(const Vector2<U>& vec)
		{
			x = static_cast<T>(vec.x);
			y = static_cast<T>(vec.y);
			return *this;
		}

		Vector2<T> operator+(const Vector2<T>& vec) const { return Vector2(x + vec.x, y + vec.y); }

		Vector2<T> operator-(const Vector2<T>& vec) const { return Vector2(x - vec.x, y - vec.y); }

		Vector2<T> operator+(T value) const
		{
			return Vector2(x + value, y + value);
		}

		Vector2<T> operator-(T value) const
		{
			return Vector2(x - value, y - value);
		}

		Vector2<T> operator-() const { return Vector2(-x, -y); }

		Vector2<T>& operator+=(const Vector2<T>& vec)
		{
			x += vec.x;
			y += vec.y;
			return *this; 
		}

		Vector2<T>& operator-=(const Vector2<T>& vec)
		{
			x -= vec.x;
			y -= vec.y;
			return *this;
		}

		bool operator==(const Vector2<T>& vec) const
		{
			return x == vec.x && y == vec.y;
		}

		bool operator!=(const Vector2<T>& vec) const
		{
			return x != vec.x || y != vec.y;
		}

		bool operator>(const Vector2<T>& vec) const
		{
			return LengthSquared() > vec.LengthSquared();
		}

		bool operator<(const Vector2<T>& vec) const
		{
			return LengthSquared() < vec.LengthSquared();
		}

		bool operator<=>(const Vector2<T>& vec)
		{
			return LengthSquared() <=> vec.LengthSquared();
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector2<T> operator*(U _u) const { return Vector2(x * _u, y * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector2<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			return *this; 
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector2<T> operator/(U _u) const
		{
			float fInv = 1 / static_cast<float>(_u);
			return Vector2(x * fInv, y * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector2<T>& operator/=(U _u)
		{
			float fInv = 1 / static_cast<float>(_u);
			x *= fInv;
			y *= fInv;
			return *this;
		}

		T operator[](uint32_t index) const
		{
			if (index == 0) return x;
			return y;
		}

		T& operator[](uint32_t index)
		{
			if (index == 0) return x;
			return y;
		}

		T LengthSquared() const { return x * x + y * y; }

		float Length() const { return std::sqrt(LengthSquared()); }

		T x, y;
	};
	
	using Vector2F = Vector2<float>;
	using Vector2I = Vector2<uint32_t>;

	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Vector3
	{
		Vector3() : x(0), y(0), z(0) {}

		explicit Vector3(T _Num) : x(_Num), y(_Num), z(_Num) {}

		Vector3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}

		explicit Vector3(const Vector4<T>& vec) : x(vec.x), y(vec.y), z(vec.z) {}

		Vector3(const Vector3<T>& vec) : x(vec.x), y(vec.y), z(vec.z) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit Vector3(const Vector3<U>& vec) :
			x(static_cast<T>(vec.x)),
			y(static_cast<T>(vec.y)),
			z(static_cast<T>(vec.z))
		{
		}

		Vector3& operator=(const Vector3<T>& vec)
		{
			x = vec.x;
			y = vec.y;
			z = vec.z;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector3& operator=(const Vector3<U>& vec)
		{
			x = static_cast<T>(vec.x);
			y = static_cast<T>(vec.y);
			z = static_cast<T>(vec.z);
			return *this;
		}

		Vector3<T> operator+(const Vector3<T>& vec) const { return Vector3(x + vec.x, y + vec.y, z + vec.z); }

		Vector3<T> operator-(const Vector3<T>& vec) const { return Vector3(x - vec.x, y - vec.y, z - vec.z); }

		Vector3<T> operator+(T value) const
		{
			return Vector3(x + value, y + value, z + value);
		}

		Vector3<T> operator-(T value) const
		{
			return Vector3(x - value, y - value, z - value);
		}

		Vector3<T> operator-() const { return Vector3(-x, -y, -z); }

		Vector3<T>& operator+=(const Vector3<T>& vec)
		{
			x += vec.x;
			y += vec.y;
			z += vec.z;
			return *this;
		}

		Vector3<T>& operator-=(const Vector3<T>& vec)
		{
			x -= vec.x;
			y -= vec.y;
			z -= vec.z;
			return *this;
		}

		bool operator==(const Vector3<T>& vec) const
		{
			return x == vec.x && y == vec.y && z == vec.z;
		}

		bool operator!=(const Vector3<T>& vec) const
		{
			return x != vec.x || y != vec.y || z != vec.z;
		}

		bool operator>(const Vector3<T>& vec) const
		{
			return LengthSquared() > vec.LengthSquared();
		}

		bool operator<(const Vector3<T>& vec) const
		{
			return LengthSquared() < vec.LengthSquared();
		}

		bool operator<=>(const Vector3<T>& vec) const
		{
			return LengthSquared() <=> vec.LengthSquared();
		}

		Vector3<T> operator*(const Vector3<T>& vec) const
		{
			return Vector3(x * vec.x, y * vec.y, z * vec.z); 
		}

		Vector3<T>& operator*=(const Vector3<T>& vec) const
		{
			return (*this) * vec;
		}


		template <typename U>
		Vector3<T> operator*(U _u) const { return Vector3(x * _u, y * _u, z * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector3<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			z *= _u;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector3<T> operator/(U _u) const
		{
			const float fInv = 1 / static_cast<float>(_u);
			return Vector3(x * fInv, y * fInv, z * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector3<T>& operator/=(U _u)
		{
			const float fInv = 1 / static_cast<float>(_u);
			x *= fInv;
			y *= fInv;
			z *= fInv;
			return *this;
		}

		T operator[](uint32_t index) const
		{
			if (index == 0) return x;
			if (index == 1) return y;
			return z;
		}

		T& operator[](uint32_t index)
		{
			if (index == 0) return x;
			if (index == 1) return y;
			return z;
		}

		T LengthSquared() const { return x * x + y * y + z * z; }

		float Length() const { return std::sqrt(LengthSquared()); }

		T x, y, z;
	};

	using Vector3F = Vector3<float>;
	using Vector3I = Vector3<uint32_t>;

	// w 分量初始化为 1
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Vector4
	{
		Vector4() : x(0), y(0), z(0), w(1) {}

		explicit Vector4(T _Num) : x(_Num), y(_Num), z(_Num), w(_Num) {}

		Vector4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) {}

		Vector4(const Vector4<T>& vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}

		explicit Vector4(const Vector3<T>& vec, T _w = 1) : x(vec.x), y(vec.y), z(vec.z), w(_w) {}

		template <typename U>
		requires std::is_arithmetic_v<U>
		explicit Vector4(const Vector4<U>& vec) :
			x(static_cast<T>(vec.x)),
			y(static_cast<T>(vec.y)),
			z(static_cast<T>(vec.z)),
			w(static_cast<T>(vec.w))
		{
		}

		Vector4& operator=(const Vector4<T>& vec)
		{
			x = vec.x;
			y = vec.y;
			z = vec.z;
			w = vec.w;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector4& operator=(const Vector4<U>& vec)
		{
			x = static_cast<T>(vec.x);
			y = static_cast<T>(vec.y);
			z = static_cast<T>(vec.z);
			w = static_cast<T>(vec.w);
			return *this;
		}

		Vector4<T> operator+(const Vector4<T>& vec) const
		{
			return Vector4(x + vec.x, y + vec.y, z + vec.z, w + vec.w);
		}

		Vector4<T> operator-(const Vector4<T>& vec) const
		{
			return Vector4(x - vec.x, y - vec.y, z - vec.z, w - vec.w);
		}

		Vector4<T> operator+(T value) const
		{
			return Vector4(x + value, y + value, z + value, w + value);
		}

		Vector4<T> operator-(T value) const
		{
			return Vector4(x - value, y - value, z - value, w - value);
		}

		Vector4<T> operator-() const { return Vector4(-x, -y, -z, -w); }

		Vector4<T>& operator+=(const Vector4<T>& vec)
		{
			x += vec.x;
			y += vec.y;
			z += vec.z;
			w += vec.w;
			return *this;
		}

		Vector4<T>& operator-=(const Vector4<T>& vec)
		{
			x -= vec.x;
			y -= vec.y;
			z -= vec.z;
			w -= vec.w;
			return *this;
		}

		bool operator==(const Vector4<T>& vec) const
		{
			return x == vec.x && y == vec.y && z == vec.z && w == vec.w;
		}

		bool operator!=(const Vector4<T>& vec) const
		{
			return x != vec.x || y != vec.y || z != vec.z ||  w != vec.w;
		}

		bool operator>(const Vector4<T>& vec) const
		{
			return LengthSquared() > vec.LengthSquared();
		}

		bool operator<(const Vector4<T>& vec) const
		{
			return LengthSquared() < vec.LengthSquared();
		}

		bool operator<=>(const Vector4<T>& vec) const
		{
			return LengthSquared() <=> vec.LengthSquared();
		}

		template <typename U>
		Vector4<T> operator*(U _u) const { return Vector4(x * _u, y * _u, z * _u, w * _u); }

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector4<T>& operator*=(U _u)
		{
			x *= _u;
			y *= _u;
			z *= _u;
			w *= _u;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector4<T> operator/(U _u) const
		{
			const float fInv = 1 / static_cast<float>(_u);
			return Vector4(x * fInv, y * fInv, z * fInv, w * fInv);
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Vector4<T>& operator/=(U _u)
		{
			const float fInv = 1 / static_cast<float>(_u);
			x *= fInv;
			y *= fInv;
			z *= fInv;
			w *= fInv;
			return *this;
		}

		T operator[](uint32_t index) const
		{
			if (index == 0) return x;
			if (index == 1) return y;
			if (index == 2) return z;
			return w;
		}

		T& operator[](uint32_t index)
		{
			if (index == 0) return x;
			if (index == 1) return y;
			if (index == 2) return z;
			return w;
		}

		T LengthSquared() const { return x * x + y * y + z * z + w * w; }

		float Length() const { return std::sqrt(LengthSquared()); }

		T x, y, z, w;
	};

	using Vector4F = Vector4<float>;
	using Vector4I = Vector4<int32_t>;

    
	template <typename U, typename T>
	inline Vector4<T> operator*(U _u, const Vector4<T>& vec)
	{
		return vec * _u;
	}
	
	template <typename U, typename T>
	inline Vector3<T> operator*(U _u, const Vector3<T>& vec)
	{
		return vec * _u;
	}
	
	template <typename U, typename T>
	inline Vector2<T> operator*(U _u, const Vector2<T>& vec)
	{
		return vec * _u;
	}
	
	template <typename T>
	inline T dot(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z;
	}
	
	template <typename T>
	inline T dot(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return vec1.x * vec2.x + vec1.y * vec2.y;
	}

	template <typename T>
	inline T Absdot(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return std::abs(dot(vec1, vec2));
	}

	template <typename T>
	inline T Absdot(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return std::abs(dot(vec1, vec2));
	}

	template <typename T>
	inline Vector3<T> cross(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		// 使用 double 可以防止两个值非常接近的浮点数相减造成的误差
		const double dVec1X = vec1.x, dVec1Y = vec1.y, dVec1Z = vec1.z;
		const double dVec2X = vec2.x, dVec2Y = vec2.y, dVec2Z = vec2.z;
		return Vector3<T>{
			static_cast<T>(dVec1Y * dVec2Z - dVec1Z * dVec2Y),
			static_cast<T>(dVec1Z * dVec2X - dVec1X * dVec2Z),
			static_cast<T>(dVec1X * dVec2Y - dVec1Y * dVec2X)
		};
	}
	
	template <typename T>
	inline Vector4<T> normalize(const Vector4<T>& vec)
	{
		return vec / vec.Length();
	}

	template <typename T>
	inline Vector3<T> normalize(const Vector3<T>& vec)
	{
		return vec / vec.Length();
	}
	
	template <typename T>
	inline Vector2<T> normalize(const Vector2<T>& vec)
	{
		return vec / vec.Length();
	}
	
	// 最小坐标值
	template <typename T>
	inline T MinComponent(const Vector3<T>& vec)
	{
		return (std::min)(vec.x, (std::min)(vec.y, vec.z));
	}

	// 最大坐标值
	template <typename T>
	inline T MaxComponent(const Vector3<T>& vec)
	{
		return (std::max)(vec.x, (std::max)(vec.y, vec.z));
	}

	// 最大坐标值索引，即投影最长的维度
	template <typename T>
	inline uint32_t MaxDimension(const Vector3<T>& vec)
	{
		return (vec.x > vec.y) ? (vec.x > vec.z ? 0 : 2) : (vec.y > vec.z ? 1 : 2);
	}

	// 最小坐标值索引，即投影最短的维度
	template <typename T>
	inline uint32_t MinDimension(const Vector3<T>& vec)
	{
		return (vec.x < vec.y) ? (vec.x < vec.z ? 0 : 2) : (vec.y < vec.z ? 1 : 2);
	}

	// 取两向量各坐标最小值
	template <typename T>
	inline Vector3<T> min(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return Vector3<T>{
			(std::min)(vec1.x, vec2.x),
			(std::min)(vec1.y, vec2.y),
			(std::min)(vec1.z, vec2.z)
		};
	}

	// 取两向量各坐标最大值
	template <typename T>
	inline Vector3<T> max(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return Vector3<T>{
			(std::max)(vec1.x, vec2.x),
			(std::max)(vec1.y, vec2.y),
			(std::max)(vec1.z, vec2.z)
		};
	}

	// 重新排列坐标
	template <typename T>
	inline Vector3<T> Permute(const Vector3<T>& vec, uint32_t _x, uint32_t _y, uint32_t _z)
	{
		return Vector3<T>(vec[_x], vec[_y], vec[_z]);
	}

	// 获取单个向量所组成的坐标系基底
	template <typename T>
	inline void CoordinateSystem(const Vector3<T>& vec1, Vector3<T>* vec2, Vector3<T>* vec3)
	{
		auto Vec1 = normalize(vec1);
		if (std::abs(Vec1.x) > std::abs(Vec1.y))
		{
			*vec2 = Vector3(-Vec1.z, 0, Vec1.x) / 
				std::sqrt(Vec1.x * Vec1.x + Vec1.z * Vec1.z);
		}
		else
		{
			*vec2 = Vector3(0, Vec1.z, -Vec1.y) /
				std::sqrt(Vec1.z * Vec1.z + Vec1.y * Vec1.y);
		}

		*vec3 = cross(vec1, *vec2);
	}

	// 两点距离
	template <typename T>
	inline float Distance(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return (vec1 - vec2).Length();
	}

	// 两点距离
	template <typename T>
	inline float Distance(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return (vec1 - vec2).Length();
	}

	// 两点距离平方
	template <typename T>
	inline float DistanceSquared(const Vector3<T>& vec1, const Vector3<T>& vec2)
	{
		return (vec1 - vec2).LengthSquared();
	}

	// 两点距离平方
	template <typename T>
	inline float DistanceSquared(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return (vec1 - vec2).LengthSquared();
	}

	// 点的插值
	template <typename T>
	inline Vector3<T> lerp(const Vector3<T>& vec1, const Vector3<T>& vec2, float f)
	{
		float fLerp = (std::min)(1.0f, (std::max)(0.0f, f));
		return (1 - fLerp) * vec1 + fLerp * vec2;
	}

	// 点的插值
	template <typename T>
	inline Vector2<T> lerp(const Vector2<T>& vec1, const Vector2<T>& vec2, float f)
	{
		return (1 - f) * vec1 + f * vec2;
	}

	// 向量的插值
	template <typename T>
	inline Vector4<T> lerp(float f, const Vector4<T>& vec1, const Vector4<T>& vec2)
	{
		float fLerp = (std::min)(1.0f, (std::max)(0.0f, f));
		return Vector4<T>((1 - fLerp) * vec1 + fLerp * vec2);
	}

	// 取两点各坐标最小值
	template <typename T>
	inline Vector2<T> min(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return Vector2<T>{
			(std::min)(vec1.x, vec2.x),
			(std::min)(vec1.y, vec2.y)
		};
	}

	// 取两点各坐标最大值
	template <typename T>
	inline Vector2<T> max(const Vector2<T>& vec1, const Vector2<T>& vec2)
	{
		return Vector2<T>{
			(std::max)(vec1.x, vec2.x),
			(std::max)(vec1.y, vec2.y),
			(std::max)(vec1.z, vec2.z)
		};
	}

	template <typename T>
	inline Vector3<T> Floor(const Vector3<T>& vec)
	{
		return Vector3<T>{std::floor(vec.x), std::floor(vec.y), std::floor(vec.z)};
	}

	template <typename T>
	inline Vector2<T> Floor(const Vector2<T>& vec)
	{
		return Vector2<T>{std::floor(vec.x), std::floor(vec.y)};
	}

	template <typename T>
	inline Vector3<T> Ceil(const Vector3<T>& vec)
	{
		return Vector3<T>{std::ceil(vec.x), std::ceil(vec.y), std::ceil(vec.z)};
	}

	template <typename T>
	inline Vector2<T> Ceil(const Vector2<T>& vec)
	{
		return Vector2<T>{std::ceil(vec.x), std::ceil(vec.y)};
	}
	
	template <typename T>
	inline Vector3<T> Abs(const Vector3<T>& vec)
	{
		return Vector3<T>{std::abs(vec.x), std::abs(vec.y), std::abs(vec.z)};
	}

	// 调整第一个向量参数的方向，使之与第二个向量参数的方向一致
	template <typename T>
	inline Vector3<T> Faceforward(const Vector3<T> &v1, const Vector3<T> &v2)
	{
		return dot(v1, v2) < 0.0f ? -v1 : v1;
	}

    	// 给定单位球面坐标系中点的仰角和方位角，使用默认基向量，返回对应的三位向量
	inline Vector3F SphericalDirection(float sin_theta, float cos_theta, float phi)
	{
		return Vector3F(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
	}

	// 给定单位球面坐标系中点的仰角和方位角，以及基向量，返回对应的三位向量
	inline Vector3F SphericalDirection(
		float sin_theta, 
		float cos_theta, 
		float phi, 
		const Vector3F& x, 
		const Vector3F& y, 
		const Vector3F& z
	)
	{
		// z = r * cos(theta)
        // x = r * sin(theta) * cos(phi)
        // y = r * sin(theta) * sin(phi)
		// 单位球面，r = 1，于是分别乘以相应的基向量
		return sin_theta * std::cos(phi) * x + sin_theta * std::sin(phi) * y + cos_theta * z;
	}

	// 计算向量 vec 与 z 轴的夹角（仰角），即球面坐标系中的 theta 角
	inline float SphericalTheta(const Vector3F& vec)
	{
		return std::acos(Clamp(vec.z, -1, 1));
	}

	// 计算向量 vec 在 xy 平面上的投影的方位角，即球面坐标系中的 phi 角
	inline float SphericalPhi(const Vector3F& vec)
	{
		// std::atan2 的值域为 [-π，π]
		float p = std::atan2(vec.y, vec.x);
		return p < 0.0f ? p + 2 * PI : p;
	}

}















#endif