#include "Graph.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace ge
{
	double normalize(const u64 value, const u64 min, const u64 max, const u64 normalized_maximum)
	{
		return ((value - min) / static_cast<f64>(max - min)) * normalized_maximum;
	}

	void draw_graph(const i8 height, const std::vector<u64>& values)
	{
		assert(height > 0);
		assert(values.size() > 2);

		std::vector<u64> normalized_values( values.size() );

		const u64 min_value = *std::min_element(values.begin(), values.end());
		const u64 max_value = *std::max_element(values.begin(), values.end());

		if (min_value == max_value)
		{
			// All of the values are the same, so normalize them to the center
			std::transform(normalized_values.begin(), normalized_values.end(), normalized_values.begin(), [height](const u64 value){
				return height / 2.0f;
			});
		}
		else
		{
			// Normalize the values normally
			std::transform(values.begin(), values.end(), normalized_values.begin(), [height, min_value, max_value](const u64 value){
				return std::round(normalize(value, min_value, max_value, height));
			});
		}

		constexpr char wall_char = '|';
		constexpr char line_char = '-';

		std::cout << '\n';

		for (i8 i = height; i > -1; --i)
		{
			for (i8 j = 0; j < normalized_values.size(); ++j)
			{
				if (normalized_values[j] != i)
				{
					std::cout << "▒";
					continue;
				}

				std::cout << "▀";
			}

			std::cout << '\n';
		}
	}
}
