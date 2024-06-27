#pragma once

#include "Types.hpp"

#include <string>

namespace ge
{
	std::string round_big_numbers(u64 value);
	std::string clean_decimals(const double value);
	u64 round(const u64 value, const i8 decimal_points);
}
