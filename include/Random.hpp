#pragma once

#include "Types.hpp"

#include <concepts>

namespace ge
{
	u64 random_seed();

	template<typename T>
	constexpr T combine_hashes(const T a, const T b)
	{
		static_assert(!std::floating_point<T>, "Floating point values cannot be used as hashes");
		return a ^ (b << 1);
	}
}
