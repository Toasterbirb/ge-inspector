#include "PriceUtils.hpp"

#include <cmath>

namespace ge
{
	std::string round_big_numbers(u64 value)
	{
		if (value > 1'000'000'000)
		{
			value = round(value, -6);
			return clean_decimals(value / 1'000'000'000.0) + "b";
		}
		else if (value > 1'000'000)
		{
			value = round(value, -4);
			return clean_decimals(value / 1'000'000.0) + "m";
		}
		else if (value > 1000)
		{
			value = round(value, -1);
			return clean_decimals(value / 1000.0) + "k";
		}

		/* Small enough value to not need rounding */
		return std::to_string(value);
	}

	std::string clean_decimals(const double value)
	{
		std::string result = std::to_string(value);
		size_t non_zero_pos = result.find_last_not_of('0');

		if (non_zero_pos != std::string::npos && non_zero_pos + 1 < result.size())
			result.erase(non_zero_pos + 1, result.size() - non_zero_pos - 1);

		/* Check if the last char is a dot */
		if (result[result.size() - 1] == '.')
			result.erase(result.size() - 1, 1);

		return result;
	}

	u64 round(const u64 value, const i8 decimal_points)
	{
		return std::round(value * std::pow(10, decimal_points)) / static_cast<f64>(std::pow(10, decimal_points));
	}
}
