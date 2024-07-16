#pragma once

#include "Types.hpp"

#include <string>

namespace ge
{
	__attribute__((warn_unused_result, const))
	std::string round_big_numbers(const u64 value);

	__attribute__((warn_unused_result, const))
	std::string clean_decimals(const double value);

	__attribute__((warn_unused_result, const))
	u64 round(const u64 value, const i8 decimal_points);
}
