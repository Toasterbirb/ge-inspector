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

	// Group different buy limits into groups to avoid unnecessary
	// fragmentation if the difference in limits is very small
	u32 buy_limit_group(const u32 buy_limit)
	{
		if (buy_limit <= 10) return 10;
		if (buy_limit <= 50) return 50;
		if (buy_limit <= 100) return 100;
		if (buy_limit <= 500) return 500;
		if (buy_limit <= 1000) return 1000;
		if (buy_limit <= 2500) return 2500;
		if (buy_limit <= 5000) return 5000;
		if (buy_limit <= 12000) return 12000;
		if (buy_limit <= 25000) return 25000;
		if (buy_limit <= 50000) return 50000;

		return buy_limit;
	}

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
			// Volume
			[&](const ge::item& item) -> bool
			{
				if (!filter.pre_filter_volume.empty())
				{
					const u32 limit_group = buy_limit_group(item.limit);

					if (!filter.pre_filter_volume.contains(limit_group))
						return false;

					return filter.pre_filter_volume.at(limit_group)
						.is_in_range(item.volume, filter.pre_filter_fuzz_factor);
				}

				return filter.volume.is_in_range(item.volume);
			},

			// Price
			[&](const ge::item& item) -> bool
			{
				if (!filter.pre_filter_volume.empty())
				{
					const u32 limit_group = buy_limit_group(item.limit);

					if (!filter.pre_filter_price.contains(limit_group))
						return false;

					return filter.pre_filter_price.at(limit_group)
						.is_in_range(item.price, filter.pre_filter_fuzz_factor);
				}

				return filter.price.is_in_range(item.price);
			},

			// Buy limit
			[&](const ge::item& item) -> bool { return filter.limit.is_in_range(item.limit); },

			// High alch
			[&](const ge::item& item) -> bool { return filter.alch.is_in_range(item.high_alch); },

			// Cost
			[&](const ge::item& item) -> bool { return filter.cost.is_in_range(item.price * item.limit); },

			// Category matching
			[&](const ge::item& item) -> bool
			{
				return filter.category == all_category_id
					? true
					: filter.category == item.category;
			},

			// Volume over limit
			[&](const ge::item& item) -> bool
			{
				return filter.volume_over_limit
					? item.volume >= item.limit
					: true;
			},

			// Profitable to alch
			[&](const ge::item& item) -> bool
			{
				return filter.find_profitable_to_alch_items
					? (item.price + nature_rune_cost) < item.high_alch
					: true;
			},

			// Minimum profit
			[&](const ge::item& item) -> bool
			{
				if (filter.min_margin_percent == 0 || filter.min_margin_profit_goal == 0)
					return true;

				return (item.price * (1 + filter.min_margin_percent * 0.01) - item.price) * item.limit >= filter.min_margin_profit_goal;
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

		const auto set_min_max_to_range = [](ge::range& range, const i64& value)
		{
			if (value < range.min)
				range.min = value;

			if (value > range.max)
				range.max = value;
		};

		constexpr range starting_range = { std::numeric_limits<i64>::max(), 0 };

		for (const ge::item& item : pre_filtered_items)
		{
			const u32 limit_group = buy_limit_group(item.limit);

			if (!filter.pre_filter_price.contains(limit_group))
				filter.pre_filter_price[limit_group] = starting_range;

			if (!filter.pre_filter_volume.contains(limit_group))
				filter.pre_filter_volume[limit_group] = starting_range;

			set_min_max_to_range(filter.pre_filter_price.at(limit_group), item.price);
			set_min_max_to_range(filter.pre_filter_volume.at(limit_group), item.volume);
		}

		const auto apply_ranges_changed_by_user = [](const ge::range src, ge::range& dst)
		{
			dst.min = src.is_min_set() ? src.min : dst.min;
			dst.max = src.is_max_set() ? src.max : dst.max;
		};

		for (auto& [limit, price] : filter.pre_filter_price)
			apply_ranges_changed_by_user(filter.price, price);

		for (auto& [limit, volume] : filter.pre_filter_volume)
			apply_ranges_changed_by_user(filter.volume, volume);

		return filter;
	}
}
