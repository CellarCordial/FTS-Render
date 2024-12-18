#ifndef CORE_ATL_COM_H
#define CORE_ATL_COM_H

#include <memory>
#include <type_traits>

template <typename T, typename U>
T check_cast(U u) noexcept
{
	static_assert(!std::is_same<T, U>::value, "Redundant checked_cast");
#ifdef DEBUG
	if (!u) return nullptr;
	T t = dynamic_cast<T>(u);
	if (!t) assert(!"invalid type cast");
	return t;
#else
	return static_cast<T>(u);
#endif
}

template <typename T, typename U>
std::shared_ptr<T> check_cast(const std::shared_ptr<U>& u) noexcept
{
	static_assert(!std::is_same<T, U>::value, "Redundant checked_cast");
#ifdef DEBUG
	if (!u) return nullptr;
	std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(u);
	if (!t) assert(!"invalid type cast");
	return t;
#else
	return std::static_pointer_cast<T>(u);
#endif
}

#endif