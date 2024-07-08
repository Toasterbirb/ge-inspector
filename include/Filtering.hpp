#pragma once

#include "Item.hpp"
#include "Types.hpp"

#include <limits>
#include <string>
#include <vector>

namespace ge
{
	struct range
	{
		i64 min = 0;
		i64 max = std::numeric_limits<i64>::max();

		__attribute__((warn_unused_result))
		bool is_min_set() const { return min > 1; }

		__attribute__((warn_unused_result))
		bool is_max_set() const { return max < std::numeric_limits<i64>::max(); }
	};

	struct filter
	{
		// Filtering ranges
		range price, volume, limit, alch, cost;
		bool find_profitable_to_alch_items = false;
		std::string name_contains;
		std::string regex_pattern;
	};

	std::vector<item> filter_items(const std::vector<item>& items, filter filter);
}
