#pragma once

#include "Types.hpp"

#include <string>

namespace ge
{
	__attribute__((warn_unused_result, const))
	std::string round_big_numbers(const i64 value);

	__attribute__((warn_unused_result, const))
	std::string clean_decimals(const f64 value);

	__attribute__((warn_unused_result, const))
	i64 round(const i64 value, const i8 decimal_points);
}
