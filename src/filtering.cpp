#include "DB.hpp"
#include "Filtering.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <regex>

namespace ge
{
	// Make sure that the filter stuff is working as intended
	constexpr range TEST_RANGE { 1000, 2000 };
	static_assert(TEST_RANGE.is_min_set());
	static_assert(TEST_RANGE.is_max_set());
	static_assert(TEST_RANGE.min == 1000);
	static_assert(TEST_RANGE.max == 2000);
	static_assert(TEST_RANGE.is_in_range(1000));
	static_assert(TEST_RANGE.is_in_range(2000));
	static_assert(TEST_RANGE.is_in_range(1500));
	static_assert(!TEST_RANGE.is_in_range(999));
	static_assert(!TEST_RANGE.is_in_range(2001));

	f64 ratio_stat_value(const item& item, const stat ratio_stat)
	{
		f64 value;

		switch (ratio_stat)
		{
			case stat::price:
				value = item.price;
				break;

			case stat::volume:
				value = item.volume;
				break;

			case stat::limit:
				value = item.limit;
				break;

			case stat::alch:
				value = item.high_alch;
				break;

			case stat::none:
				assert(true == false && "This code path shouldn't get reached");
				break;
		}

		return value;
	}

	std::vector<item> filter_items(const std::vector<item>& items, filter filter)
	{
		// Check the cost of a nature rune (we are assuming that a fire battlestaff is used)
		// This variable is only really used if we are checking for alching profitability
		u64 nature_rune_cost = filter.find_profitable_to_alch_items ? ge::item_cost("Nature rune") : 0;

		// Convert the name contains filter to lowercase
		std::transform(filter.name_contains.begin(), filter.name_contains.end(), filter.name_contains.begin(), [](unsigned char c) {
			return std::tolower(c);
		});

		static const u8 all_category_id = ge::item_categories.at("All");

		// Check if ratio filtering should done
		bool ratio_filtering_enabled = filter.ratio_stat_a != stat::none && filter.ratio_stat_b != stat::none;

		std::vector<item> result;

		const static std::vector<std::function<bool(const ge::item& item)>> filter_funcs = {
			// Category matching
			[&](const ge::item& item) -> bool
			{
				return filter.category == all_category_id
					? true
					: filter.category == item.category;
			},

			// Price
			[&](const ge::item& item) -> bool { return filter.price.is_in_range(item.price); },

			// Buy limit
			[&](const ge::item& item) -> bool { return filter.limit.is_in_range(item.limit); },

			// Volume
			[&](const ge::item& item) -> bool { return filter.volume.is_in_range(item.volume); },

			// High alch
			[&](const ge::item& item) -> bool { return filter.alch.is_in_range(item.high_alch); },

			// Cost
			[&](const ge::item& item) -> bool { return filter.cost.is_in_range(item.price * item.limit); },

			// Volume over limit
			[&](const ge::item& item) -> bool { return filter.volume_over_limit ? item.volume >= item.limit : true; },

			// Profitable to alch
			[&](const ge::item& item) -> bool
			{
				return filter.find_profitable_to_alch_items ? (item.price + nature_rune_cost) < item.high_alch : true;
			},

			// Stat ratio filter
			[&](const ge::item& item) -> bool
			{
				if (!ratio_filtering_enabled)
					return true;

				f64 value_a = ratio_stat_value(item, filter.ratio_stat_a);
				f64 value_b = ratio_stat_value(item, filter.ratio_stat_b);

				return value_a >= value_b * filter.stat_ratio;
			},

			// Name filter
			[&](const ge::item& item) -> bool
			{
				// Check if the name filtering was used
				if (filter.name_contains.empty())
					return true;

				// Convert everything to lowercase before doing the comparisons
				std::string lowercase_name;

				std::transform(item.name.begin(), item.name.end(), std::back_inserter(lowercase_name), [](unsigned char c) {
					return std::tolower(c);
				});

				return lowercase_name.find(filter.name_contains) != std::string::npos;
			},

			// Regex matching
			[&](const ge::item& item) -> bool
			{
				// Check if regex matching was used
				if (filter.regex_patterns.empty())
					return true;

				// Check if all regex ptterns match
				return std::all_of(filter.regex_patterns.begin(), filter.regex_patterns.end(), [&](const std::string& pattern){
					return std::regex_match(item.name, std::regex(pattern));
				});
			}
		};

		std::copy_if(items.begin(), items.end(), std::back_inserter(result), [&](const ge::item& item){
			// Execute filtering functions
			return std::all_of(filter_funcs.begin(), filter_funcs.end(), [&](const auto& func) { return func(item); });
		});

		return result;
	}

	// Function stolen from subst
	std::vector<std::string> tokenize_string(const std::string& line, const char separator)
	{
		std::vector<std::string> tokens;

		std::istringstream line_stream(line);
		std::string token;

		while (std::getline(line_stream, token, separator))
			tokens.push_back(token);

		return tokens;
	}

	filter pre_filter(const filter& user_provided_values, const std::vector<item>& items, const std::string& pre_filter_item_names)
	{
		filter filter = user_provided_values;

		std::vector<std::string> item_names = tokenize_string(pre_filter_item_names, ';');

		// Determine the min and max values from the given items
		std::vector<ge::item> pre_filtered_items;
		for (const std::string& pre_filter_item_name : item_names)
		{
			std::copy_if(items.begin(), items.end(), std::back_inserter(pre_filtered_items), [pre_filter_item_name](const ge::item& item){
				return pre_filter_item_name == item.name;
			});
		}

		ge::range pre_price { std::numeric_limits<i64>::max(), 0 };
		ge::range pre_volume { std::numeric_limits<i64>::max(), 0 };
		ge::range pre_limit { std::numeric_limits<i64>::max(), 0 };

		const auto set_min_max_to_range = [](ge::range& range, const i64& value)
		{
			if (value < range.min)
				range.min = value;

			if (value > range.max)
				range.max = value;
		};

		for (const ge::item& item : pre_filtered_items)
		{
			set_min_max_to_range(pre_price, item.price);
			set_min_max_to_range(pre_volume, item.volume);
			set_min_max_to_range(pre_limit, item.limit);
		}

		const auto apply_ranges_not_changed_by_user = [](const ge::range src, ge::range& dst)
		{
			dst.min = dst.is_min_set() ? dst.min : src.min;
			dst.max = dst.is_max_set() ? dst.max : src.max;
		};

		apply_ranges_not_changed_by_user(pre_price, filter.price);
		apply_ranges_not_changed_by_user(pre_volume, filter.volume);
		apply_ranges_not_changed_by_user(pre_limit, filter.limit);

		return filter;
	}
}
