#ifndef MATH_BOUNDS_H
#define MATH_BOUNDS_H

#include "vector.h"
#include "ray.h"
#include <cassert>
#include <cstdint>
#include <vector>

namespace fantasy 
{
    template <typename T> requires std::is_arithmetic_v<T> struct Bounds2;
    template <typename T> requires std::is_arithmetic_v<T> struct Bounds3;

	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Bounds2
	{
		Bounds2()
		{
			T max = std::numeric_limits<T>::max();
			T min = std::numeric_limits<T>::lowest();

			_lower = Vector2<T>(max, max);
			_upper = Vector2<T>(min, min);
		}

		explicit Bounds2(const Vector2<T>& vec) : _lower(vec), _upper(vec) {}
		
		Bounds2(const Vector2<T>& vec0, const Vector2<T>& vec1)
		{
			_upper = max(vec0, vec1);
			_lower = min(vec0, vec1);
		}

		explicit Bounds2(const std::vector<Vector2<T>>& vertices)
		{
			T min_x = vertices[0].x, min_y = vertices[0].y;
			T max_x = vertices[0].x, max_y = vertices[0].y;
			for (const auto& vertex : vertices)
			{
				_lower = Vector2<T>(
					std::min(min_x, vertex.x),
					std::min(min_y, vertex.y)
				);
				_upper = Vector2<T>(
					std::max(max_x, vertex.x),
					std::max(max_y, vertex.y)
				);
			}
		}

		const Vector2<T>& operator[](uint32_t index) const
		{
			// FCHECK(index <= 1);
			if (index == 0) return _lower;
			return _upper;
		}

		Vector2<T>& operator[](uint32_t index)
		{
			// FCHECK(index <= 1);
			if (index == 0) return _lower;
			return _upper;
		}

		bool operator==(const Bounds2<T>& other)
		{
			return _upper == other._upper && _lower == other._lower;
		}
		
		bool operator!=(const Bounds2<T>& other)
		{
			return _upper != other._upper || _lower != other._lower;
		}

		template <typename U>
		explicit operator Bounds2<U>() const
		{
			return Bounds2<U>(Vector2<U>(_lower), Vector2<U>(_upper));
		}

		// 返回沿框的对角线从最小点指向最大点的向量
		Vector2<T> diagonal() const { return _upper - _lower; }

		T width() const
		{
			return _upper.x - _lower.x;
		}

		T height() const
		{
			return _upper.y - _lower.y;
		}

		T area() const
		{
			Vector2<T> vec = diagonal();
			return vec.x * vec.y;
		}

		// 返回三轴中最长轴的索引
		uint32_t max_extent() const
		{
			Vector2<T> vec = diagonal();
			if (vec.x > vec.y) return 0;
			return 1;
		}

		// 每个维度的给定量在框的两个顶点之间线性插值，获取一个指向包围盒内部的向量
		Vector2<T> lerp(const Vector2<T>& vec)
		{
			return Vector2<T>(
				lerp(vec.x, _lower.x, _upper.x),
				lerp(vec.y, _lower.y, _upper.y)
			);
		}

		// 获取点相对包围盒最小顶点的偏移，并归一化
		Vector2<T> offset(const Vector2<T>& vec)
		{
			Vector2<T> o = vec - _lower;
			if (_upper.x > _lower.x) o.x /= _upper.x - _lower.x;
			if (_upper.y > _lower.y) o.y /= _upper.y - _lower.y;
			return o;
		}

		void bounding_sphere(Vector2<T>* pCenter, float* pfRadius)
		{
			*pCenter = (_lower + _upper) / 2;
			*pfRadius = inside(*pCenter, *this) ? distance(*pCenter, _upper) : 0;
		}
		
		Vector2<T> _lower, _upper;
	};

	using Bounds2F = Bounds2<float>;
	using Bounds2I = Bounds2<int32_t>;

	using Bounds3F = Bounds3<float>;
	using Bounds3I = Bounds3<int32_t>;
	
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Bounds3
	{
		Bounds3()
		{
			T max = std::numeric_limits<T>::max();
			T min = std::numeric_limits<T>::lowest();

			_lower = Vector3<T>(max, max, max);
			_upper = Vector3<T>(min, min, min);
		}

		Bounds3(T min, T max) :
			_lower(min, min, min),
			_upper(max, max, max)
		{
		}

		Bounds3(T min_x, T min_y, T min_z, T max_x, T max_y, T max_z) :
			_lower(min_x, min_y, min_z),
			_upper(max_x, max_y, max_z)
		{
		}

		explicit Bounds3(const Vector3<T>& vec) : _lower(vec), _upper(vec) {}
		
		explicit Bounds3(const std::vector<Vector3<T>>& vertices)
		{
			T min_x = vertices[0].x, min_y = vertices[0].y, min_z = vertices[0].z;
			T max_x = vertices[0].x, max_y = vertices[0].y, max_z = vertices[0].z;
			for (const auto& vertex : vertices)
			{
				min_x = std::min(min_x, vertex.x);
				min_y = std::min(min_y, vertex.y);
				min_z = std::min(min_z, vertex.z);
				max_x = std::max(max_x, vertex.x);
				max_y = std::max(max_y, vertex.y);
				max_z = std::max(max_z, vertex.z);
			}
			_lower = Vector3<T>(min_x, min_y, min_z);
			_upper = Vector3<T>(max_x, max_y, max_z);
		}
		

		/**
		 * @brief       构造函数, 无需在意两者最大最小点之分
		 * 
		 * @param       vec0  
		 * @param       vec1 
		 */
		Bounds3(const Vector3<T>& vec0, const Vector3<T>& vec1)
		{
			_upper = max(vec0, vec1);
			_lower = min(vec0, vec1);
		}

		const Vector3<T>& operator[](uint32_t index) const
		{
			// FCHECK(index <= 1);
			if (index == 0) return _lower;
			return _upper;
		}

		Vector3<T>& operator[](uint32_t index)
		{
			// FCHECK(index <= 1);
			if (index == 0) return _lower;
			return _upper;
		}

		bool operator==(const Bounds3<T>& other) const
		{
			return _upper == other._upper && _lower == other._lower;
		}
		
		bool operator!=(const Bounds3<T>& other) const
		{
			return _upper != other._upper || _lower != other._lower;
		}

		Bounds3<T>& operator=(const Bounds3<T>& other)
		{
			_lower = other._lower;
			_upper = other._upper;
			return *this;
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Bounds3<T> operator*(U value)
		{
			_lower *= value;
			_upper *= value;
			return Bounds3<T>{ ._lower = _lower, ._upper = _upper };
		}

		template <typename U>
		requires std::is_arithmetic_v<U>
		Bounds3<T>& operator*=(U value)
		{
			_lower *= value;
			_upper *= value;
			return *this;
		}

		template <typename U>
		explicit operator Bounds3<U>() const
		{
			return Bounds3<U>(Vector3<U>(_lower), Vector3<U>(_upper));
		}

		// 获取包围盒八个顶点之一的坐标，Corner 属于 [0, 7], 分别对应8个顶点
		Vector3<T> corner(uint32_t cor) const
		{
			return Vector3<T>(
				(*this)[cor & 1].x,
				(*this)[cor & 2 ? 1 : 0].y,
				(*this)[cor & 4 ? 1 : 0].z
			);
		}

		// 返回沿框的对角线从最小点指向最大点的向量
		Vector3<T> diagonal() const { return _upper - _lower; }

		T surface_area() const
		{
			Vector3<T> vec = diagonal();
			return 2 * (vec.x * vec.y + vec.x * vec.z + vec.y * vec.z);
		}

		T volume() const
		{
			Vector3<T> vec = diagonal();
			return vec.x * vec.y * vec.z;
		}

		// 返回三轴中最长轴的索引
		uint32_t max_axis() const
		{
			Vector3<T> vec = diagonal();
			if (vec.x > vec.y && vec.x > vec.z) return 0;
			if (vec.y > vec.z) return 1;
			return 2;
		}

		float3 extent() const
		{
			return _upper - _lower;
		}

		// 每个维度的给定量在框的两个顶点之间线性插值，获取一个指向包围盒内部的向量
		Vector3<T> lerp(const Vector3<T>& vec) const
		{
			return Vector3<T>(
				lerp(vec.x, _lower.x, _upper.x),
				lerp(vec.y, _lower.y, _upper.y),
				lerp(vec.z, _lower.z, _upper.z)
			);
		}

		// 获取点相对包围盒最小顶点的偏移，并归一化
		Vector3<T> offset(const Vector3<T>& vec) const
		{
			Vector3<T> o = vec - _lower;
			if (_upper.x > _lower.x) o.x /= _upper.x - _lower.x;
			if (_upper.y > _lower.y) o.y /= _upper.y - _lower.y;
			if (_upper.z > _lower.z) o.z /= _upper.z - _lower.z;
			return o;
		}

		void bounding_sphere(Vector3<T>* center, float* out_radius) const
		{
			*center = (_lower + _upper) / 2;
			*out_radius = inside(*center, *this) ? distance(*center, _upper) : 0;
		}

		
		bool intersect(const Ray& ray, float* hit0, float* hit1) const
		{
			// 平面方程：已知空间中一个点和该点所在平面的法向量，即可确定该平面
			// 设平面上一点 $M_0(x_0, y_0, z_0)$, 另一点为 $M(x, y, z)$, 平面法线为 $\vec {n} = (a, b, c)$
			// 则有 $\vec {M_0 M} \cdot \vec {n} = 0$
			// 即为 $a(x - x_0) + b(y - y_0) + c(z - z_0) = 0$, 同 ax + by + cz + _dir = 0

			// 将光线方程代入 t 作为平面交点，则变为 $a(o_x + td_x) + b(o_y + td_y) + c(o_z + td_z) = 0$
			// 其中 $-d_{x/y/z}$ 为包围盒最大/小点
			// 化简得 $t = \frac {-d - (a, b, c) \cdot \vec {o}}{(a, b, c) \cdot \vec {d}}$
			// 以垂直于 x 轴的法线为例，其 y z 分量都为 0, 则上式可以化简为 $t = \frac {-d - o_x}{d_x}$


			float fT0 = 0.0f, fT1 = ray.max;	// 与包围盒的两个相交点

			// 对包围盒每对平面求交点
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				// 根据公式 $t = \frac {-d - o_x}{d_x}$, 先求 $d_{x/y/z} $ 的倒数

				// 若 ray._dir[ix], 即光线 ray 与正在求交的的平面平行或在平面内，则 fInvDir 会变为 INF, 这样也能正确工作
				float fInvDir = 1.0f / ray.dir[ix];
				float fTNear = (_lower[ix] - ray.ori[ix]) * fInvDir;
				float fTFar = (_upper[ix] - ray.ori[ix]) * fInvDir;

				if (fTNear > fTFar) std::swap(fTNear, fTFar);

				// 保证交点计算的精度
				// 考虑浮点误差的计算 a = (b - c) * (1 / d) 中有三个运算符 
				// 故 n 应为 3, 使用 gamma(3)
				// 至于为什么是两倍的 gamma(3), 这需要考虑 $t_{min}$ $t_{max}$ 的重叠问题
				// 当 $t_{max}$ 的误差区间和 $t_{min}$ 的误差区间交叠，则比较 $t_{min} < t_{max}$ 可能不能实际反映出光线是否命中边界框
				// 这种情况下保守地返回真比错过确有的相交更好，用tmax 误差的两倍对其扩展保证比较是保守的
				// 如下图
				//			$t_{max}$
				//    |———————————·———————————|
				// ———————————————·———·——————————————————
				//				|—————·—————|
				//				 $t_{min}$
				fTFar *= 1 + 2 * gamma(3);
				
				// 迭代比较
				fT0 = fTNear > fT0 ? fTNear : fT0;
				fT1 = fTFar < fT1 ? fTFar : fT1;

				// 无交点
				if (fT0 > fT1) return false;
			}

			if (hit0 != nullptr) *hit0 = fT0;
			if (hit1 != nullptr) *hit1 = fT1;

			return true;
		}

		// 检查光线 ray 是否与该包围盒相交，与上面的 intersect() 方法相比，该方法能带来很大的性能提升
		// crRayInvDir: 预计算的光线方向三个分量的倒数
		// dwDirIsNeg: 光线三个分量是否是原本方向的负，0 代表与原本方向一致，1 代表与原本方向相反
		bool intersect(const Ray& ray, const float3& inv_ray_dir, const uint32_t dir_is_neg[3]) const
		{
			const Bounds3F& bounds = *this;

			// 公式 $t = \frac {-d - o_x}{d_x}$

			const float min_x = (bounds[dir_is_neg[0]].x - ray.ori.x) / inv_ray_dir.x;
			float max_x = (bounds[1 - dir_is_neg[0]].x - ray.ori.x) / inv_ray_dir.x;
			
			const float min_y = (bounds[dir_is_neg[1]].y - ray.ori.y) / inv_ray_dir.y;
			float max_y = (bounds[1 - dir_is_neg[1]].y - ray.ori.y) / inv_ray_dir.y;

			// 看 1610 行
			max_x *= 1 + 2 * gamma(3);
			max_y *= 1 + 2 * gamma(3);

			float fT0 = min_x, fT1 = max_x;	// 与包围盒的两个相交点

			// 三个分量迭代比较转变为取三个中最小和最大的，但小的不能比大的大，反过来同理
			if (fT0 > max_y || fT1 < min_y) return false;
			if (fT0 < min_y) fT0 = min_y;
			if (fT1 > max_y) fT1 = max_y;

			// 同上
			const float min_z = (bounds[dir_is_neg[2]].z - ray.ori.z) / inv_ray_dir.z;
			float max_z = (bounds[1 - dir_is_neg[2]].z - ray.ori.z) / inv_ray_dir.z;

			// 看 1610 行
			max_z *= 1 + 2 * gamma(3);

			if (fT0 > max_z || fT1 < min_z) return false;
			if (fT0 < min_z) fT0 = min_z;
			if (fT1 > max_z) fT1 = max_z;

			// 确保合理
			return fT0 < ray.max && fT1 > 0;
		}

		Vector3<T> _lower, _upper;
	};

	
    struct Circle
    {
		Circle() = default;
		
		Circle(const float2& in_center, float in_radius) : center(in_center), radius(in_radius) {}

		explicit Circle(const std::vector<float2>& vertices)
		{
			Bounds2F bounding_box(vertices);
			center = (bounding_box._lower + bounding_box._upper) * 0.5f;
			radius = bounding_box.diagonal().length() * 0.5f;
		}

        float2 center;
        float radius;
    };

    struct Sphere
    {
		Sphere() = default;

		Sphere(const float3& in_center, float in_radius) : center(in_center), radius(in_radius) {}

		explicit Sphere(const std::vector<float3>& vertices)
		{
			uint32_t min_idx[3] = {};
			uint32_t max_idx[3] = {};
			for (uint32_t i = 0; i < vertices.size(); i++) 
			{
				for (int k = 0; k < 3; k++) 
				{
					if (vertices[i][k] < vertices[min_idx[k]][k]) min_idx[k] = i;
					if (vertices[i][k] > vertices[max_idx[k]][k]) max_idx[k] = i;
				}
			}

			float max_len = 0;
			uint32_t max_axis = 0;
			for (uint32_t k = 0; k < 3; k++) 
			{
				float3 pmin = vertices[min_idx[k]];
				float3 pmax = vertices[max_idx[k]];
				float tlen = float3(pmax - pmin).length_squared();
				if (tlen > max_len) max_len = tlen, max_axis = k;
			}

			float3 pmin = vertices[min_idx[max_axis]];
			float3 pmax = vertices[max_idx[max_axis]];

			center = (pmin + pmax) * 0.5f;
			radius = float(0.5 * sqrt(max_len));
			max_len = radius * radius;

			for (uint32_t i = 0; i < vertices.size(); i++) 
			{
				float len = float3(vertices[i] - center).length_squared();
				if (len > max_len) 
				{
					len = sqrt(len);
					float t = 0.5 - 0.5 * (radius / len);
					center = center + (vertices[i] - center) * t;
					radius = (radius + len) * 0.5;
					max_len = radius * radius;
				}
			}

			for (uint32_t i = 0; i < vertices.size(); i++) 
			{
				float len = float3(vertices[i] - center).length();
				assert(len - 1e-6 <= radius);
			}
		}

        float3 center;
        float radius;
    };

    
	// 组合包围盒和点
	template <typename T>
	Bounds3<T> merge(const Bounds3<T>& bounds, const Vector3<T>& vec)
	{
		return Bounds3<T>(min(bounds._lower, vec), max(bounds._upper, vec));
	}

	// 组合两个包围盒
	template <typename T>
	Bounds3<T> merge(const Bounds3<T>& bounds1, const Bounds3<T>& bounds2)
	{
		return Bounds3<T>(min(bounds1._lower, bounds2._lower), max(bounds1._upper, bounds2._upper));
	}

	// 组合包围盒和点
	template <typename T>
	Bounds2<T> merge(const Bounds2<T>& bounds, const Vector2<T>& vec)
	{
		return Bounds2<T>(min(bounds._lower, vec), max(bounds._upper, vec));
	}

	// 组合两个包围盒
	template <typename T>
	Bounds2<T> merge(const Bounds2<T>& bounds1, const Bounds2<T>& bounds2)
	{
		return Bounds2<T>(min(bounds1._lower, bounds2._lower), max(bounds1._upper, bounds2._upper));
	}

	Circle merge(const Circle& circle0, const Circle& circle1);
	Sphere merge(const Sphere& sphere0, const Sphere& sphere1);
	Sphere merge(const std::vector<Sphere>& spheres);

	// 获取两个包围盒相交处的包围盒
	template <typename T>
	Bounds3<T> intersect_box(const Bounds3<T>& bounds1, const Bounds3<T>& bounds2)
	{
		return Bounds3<T>(max(bounds1._lower, bounds2._lower), min(bounds1._upper, bounds2._upper));
	}

	// 获取两个包围盒相交处的包围盒
	template <typename T>
	Bounds2<T> intersect_box(const Bounds2<T>& bounds1, const Bounds2<T>& bounds2)
	{
		return Bounds2<T>(max(bounds1._lower, bounds2._lower), min(bounds1._upper, bounds2._upper));
	}

	template <typename T>
	bool intersect(const Bounds3<T>& bounds1, const Bounds3<T>& bounds2)
	{
		return	bounds1._lower.x < bounds2._upper.x && bounds1._upper.x > bounds2._lower.x &&
				bounds1._lower.y < bounds2._upper.y && bounds1._upper.y > bounds2._lower.y &&
				bounds1._lower.z < bounds2._upper.z && bounds1._upper.z > bounds2._lower.z;
	}

	template <typename T>
	bool intersect(const Bounds2<T>& bounds1, const Bounds2<T>& bounds2)
	{
		return	bounds1._lower.x < bounds2._upper.x && bounds1._upper.x > bounds2._lower.x &&
				bounds1._lower.y < bounds2._upper.y && bounds1._upper.y > bounds2._lower.y;
	}

	// 判断是否相交/重叠
	template <typename T>
	bool overlaps(const Bounds3<T>& bounds1, const Bounds3<T>& bounds2)
	{
		const bool x = bounds1._upper.x > bounds2._lower.x && bounds1._lower.x < bounds2._upper.x;
		const bool y = bounds1._upper.y > bounds2._lower.y && bounds1._lower.y < bounds2._upper.y;
		const bool z = bounds1._upper.z > bounds2._lower.z && bounds1._lower.x < bounds2._upper.z;

		return x && y && z;
	}

	// 判断是否相交/重叠
	template <typename T>
	bool overlaps(const Bounds2<T>& bounds1, const Bounds2<T>& bounds2)
	{
		const bool x = bounds1._upper.x > bounds2._lower.x && bounds1._lower.x < bounds2._upper.x;
		const bool y = bounds1._upper.y > bounds2._lower.y && bounds1._lower.y < bounds2._upper.y;
		return x && y;
	}

	// 判断点是否在包围盒内
	template <typename T>
	bool inside(const Vector3<T>& vec, const Bounds3<T>& bound)
	{
		return	vec.x >= bound._lower.x && vec.x <= bound.m_mMax.x &&
				vec.y >= bound._lower.y && vec.y <= bound.m_mMax.y &&
				vec.z >= bound._lower.z && vec.z <= bound.m_mMax.z;
	}

	// 判断点是否在包围盒内
	template <typename T>
	bool inside(const Vector2<T>& vec, const Bounds2<T>& bound)
	{
		return	vec.x >= bound._lower.x && vec.x <= bound.m_mMax.x &&
				vec.y >= bound._lower.y && vec.y <= bound.m_mMax.y;
	}
	
	// 判断点是否在包围盒内，不包含上边界
	template <typename T>
	bool inside_exclusive(const Vector3<T>& vec, const Bounds3<T>& bound)
	{
		return	vec.x >= bound._lower.x && vec.x < bound.m_mMax.x &&
				vec.y >= bound._lower.y && vec.y < bound.m_mMax.y &&
				vec.z >= bound._lower.z && vec.z < bound.m_mMax.z;
	}

	// 判断点是否在包围盒内，不包含上边界
	template <typename T>
	bool inside_exclusive(const Vector2<T>& vec, const Bounds2<T>& bound)
	{
		return	vec.x >= bound._lower.x && vec.x < bound.m_mMax.x &&
				vec.y >= bound._lower.y && vec.y < bound.m_mMax.y;
	}

	// 扩充包围盒边界框
	template <typename T, typename U>
	Bounds3<T> expand(const Bounds3<T>& bound, U delta)
	{
		return Bounds3<T>(
			bound._lower - Vector3<U>(delta),
			bound._upper + Vector3<U>(delta)
		);
	}

	// 扩充包围盒边界框
	template <typename T, typename U>
	Bounds2<T> expand(const Bounds2<T>& bound, U delta)
	{
		return Bounds2<T>(
			bound._lower - Vector2<U>(delta),
			bound._upper + Vector2<U>(delta)
		);
	}

    Bounds3F create_aabb(const std::vector<float3>& position);

}







#endif