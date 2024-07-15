#include "DB.hpp"
#include "Filtering.hpp"

#include <algorithm>
#include <cassert>
#include <regex>

namespace ge
{
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

		static const u8 all_category_id = ge::item_categories.at("All");

		// Check if ratio filtering should done
		bool ratio_filtering_enabled = filter.ratio_stat_a != stat::none && filter.ratio_stat_b != stat::none;

		std::vector<item> result;

		std::copy_if(items.begin(), items.end(), std::back_inserter(result), [&](const ge::item& item){
			bool generic_filters = item.price >= filter.price.min
						&& item.price <= filter.price.max
						&& (item.price * item.limit) <= filter.cost.max
						&& (item.price * item.limit) >= filter.cost.min
						&& item.volume >= filter.volume.min
						&& item.volume <= filter.volume.max
						&& item.limit >= filter.limit.min
						&& item.limit <= filter.limit.max
						&& item.high_alch >= filter.alch.min
						&& item.high_alch <= filter.alch.max;

			// Avoid the more costly checks if the generic filters already don't match
			if (!generic_filters)
				return false;

			bool name_filter = filter.name_contains.empty();
			if (!filter.name_contains.empty())
			{
				// Convert everything to lowercase before doing the comparisons
				std::string lowercase_name;

				std::transform(item.name.begin(), item.name.end(), std::back_inserter(lowercase_name), [](unsigned char c) {
					return std::tolower(c);
				});

				std::transform(filter.name_contains.begin(), filter.name_contains.end(), filter.name_contains.begin(), [](unsigned char c) {
					return std::tolower(c);
				});

				name_filter = lowercase_name.find(filter.name_contains) != std::string::npos;
			}

			bool regex_match = true;
			if (!filter.regex_patterns.empty())
			{
				// Check if all regex ptterns match
				for (const std::string& pattern : filter.regex_patterns)
				{
					if (!std::regex_match(item.name, std::regex(pattern)))
					{
						regex_match = false;
						break;
					}
				}
			}

			bool profitable_to_alch = filter.find_profitable_to_alch_items ? (item.price + nature_rune_cost) < item.high_alch : true;
			bool volume_over_limit = filter.volume_over_limit ? item.volume >= item.limit : true;

			bool category_match = filter.category == all_category_id
				? true
				: filter.category == item.category;

			bool ratio_filter = true;
			if (ratio_filtering_enabled)
			{
				f64 value_a = ratio_stat_value(item, filter.ratio_stat_a);
				f64 value_b = ratio_stat_value(item, filter.ratio_stat_b);

				ratio_filter = value_a >= value_b * filter.stat_ratio;
			}

			return generic_filters && name_filter && regex_match && profitable_to_alch && category_match && volume_over_limit && ratio_filter;
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
