#pragma once

#include "Item.hpp"
#include "Types.hpp"

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace ge
{
	struct range
	{
		i64 min = 0;
		i64 max = std::numeric_limits<i64>::max();

		__attribute__((warn_unused_result))
		constexpr bool is_min_set() const { return min > 1; }

		__attribute__((warn_unused_result))
		constexpr bool is_max_set() const
		{
			return max < std::numeric_limits<i64>::max();
		}

		__attribute__((warn_unused_result))
		constexpr bool is_in_range(const i64 value) const
		{
			return value >= min && value <= max;
		}

		__attribute__((warn_unused_result))
		constexpr bool is_in_range(const i64 value, const f32 fuzz) const
		{
			return value >= min - (min * fuzz)
				&& value <= max + (max * fuzz);
		}
	};

	enum class stat
	{
		price, volume, limit, alch, none
	};

	const static inline std::unordered_map<std::string, stat> str_to_stat = {
		{ "price", stat::price },
		{ "volume", stat::volume },
		{ "limit", stat::limit },
		{ "alch", stat::alch },
	};

	struct filter
	{
		// Filtering ranges
		range price, volume, limit, alch, cost;

		bool find_profitable_to_alch_items = false;
		bool volume_over_limit = false;

		stat ratio_stat_a = stat::none;
		stat ratio_stat_b = stat::none;
		f32 stat_ratio = 1.0f;

		f32 min_margin_percent = 0;
		f32 min_margin_profit_goal = 0;

		std::vector<std::string> name_contains;
		std::vector<std::string> regex_patterns;
		u8 category = ge::item_categories.at(all_category);

		// Group pre-filter data based on buy limits
		std::unordered_map<u32, range> pre_filter_price;
		std::unordered_map<u32, range> pre_filter_volume;
		f32 pre_filter_fuzz_factor = 0.05;
	};

	std::vector<item> filter_items(const std::vector<item>& items, filter filter);
	std::vector<std::string> tokenize_string(const std::string& line, const char separator);
	filter pre_filter(const filter& user_provided_values, const std::vector<item>& items, const std::string& pre_filter_item_names);
}
