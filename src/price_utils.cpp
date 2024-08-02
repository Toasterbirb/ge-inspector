#include "PriceUtils.hpp"

#include <cmath>

namespace ge
{
	std::string round_big_numbers(const i64 value)
	{
		constexpr i64 billion = 1'000'000'000;
		constexpr i64 million = 1'000'000;
		constexpr i64 thousand = 1000;

		if (value > billion || value < -billion) [[unlikely]]
			return clean_decimals(round(value, -6) / static_cast<f64>(billion)) + "b";
		else if (value > million || value < -million) [[likely]]
			return clean_decimals(round(value, -4) / static_cast<f64>(million)) + "m";
		else if (value > thousand || value < -thousand) [[likely]]
			return clean_decimals(round(value, -1) / static_cast<f64>(thousand)) + "k";

		/* Small enough value to not need rounding */
		return std::to_string(value);
	}

	std::string clean_decimals(const f64 value)
	{
		std::string result = std::to_string(value);
		const size_t non_zero_pos = result.find_last_not_of('0');

		if (non_zero_pos != std::string::npos && non_zero_pos + 1 < result.size())
			result.erase(non_zero_pos + 1, result.size() - non_zero_pos - 1);

		/* Check if the last char is a dot */
		if (result[result.size() - 1] == '.') [[unlikely]]
			result.erase(result.size() - 1, 1);

		return result;
	}

	i64 round(const i64 value, const i8 decimal_points)
	{
		return std::round(value * std::pow(10, decimal_points)) / static_cast<f64>(std::pow(10, decimal_points));
	}
}
