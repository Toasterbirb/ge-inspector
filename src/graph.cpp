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



		constexpr char symbol_up = '/';
		constexpr char symbol_down = '\\';
		constexpr char symbol_stationary = '_';
		constexpr char symbol_turnaround_down = '^';
		constexpr char symbol_turnaround_up = 'v';

		constexpr char wall_char = '|';
		constexpr char line_char = '-';

		// Print the top line
		for (size_t i = 0; i < normalized_values.size() + 2; ++i)
			std::cout << line_char << line_char;
		std::cout << '\n';

		for (i8 i = height; i > -1; --i)
		{
			std::cout << wall_char;

			for (i8 j = 0; j < normalized_values.size(); ++j)
			{
				if (normalized_values[j] == i)
				{
					const i8 cur_value = normalized_values[j];

					const i8 prev_value = j == 0
						? normalized_values[j]
						: normalized_values[j - 1];

					const i8 next_value = j == normalized_values.size() - 1
						? normalized_values[j]
						: normalized_values[j + 1];

					const bool increasing = cur_value > prev_value && cur_value < next_value;
					const bool decreasing = cur_value < prev_value && cur_value > next_value;
					const bool stationary = cur_value == prev_value || cur_value == next_value;
					const bool turnaround_up = prev_value == next_value && cur_value < prev_value;
					const bool turnaround_down = prev_value == next_value && cur_value > prev_value;

					const char symbol 	= symbol_up * increasing
										+ symbol_down * decreasing
										+ symbol_stationary * stationary
										+ symbol_turnaround_up * turnaround_up
										+ symbol_turnaround_down * turnaround_down;

					std::cout << ' ' << symbol;
				}
				else
				{
					std::cout << "  ";
				}
			}

			std::cout << "  " << wall_char << '\n';
		}

		// Print the bottom line
		for (size_t i = 0; i < normalized_values.size() + 2; ++i)
			std::cout << line_char << line_char;

		std::cout << '\n';
	}
}
