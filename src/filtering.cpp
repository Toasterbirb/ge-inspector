#include "DB.hpp"
#include "Filtering.hpp"

#include <algorithm>
#include <regex>

namespace ge
{
	std::vector<item> filter_items(const std::vector<item>& items, filter filter)
	{
		// Check the cost of a nature rune (we are assuming that a fire battlestaff is used)
		// This variable is only really used if we are checking for alching profitability
		u64 nature_rune_cost = filter.find_profitable_to_alch_items ? ge::item_cost("Nature rune") : 0;

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

			bool regex_match = filter.regex_pattern.empty() ? true : std::regex_match(item.name, std::regex(filter.regex_pattern));
			bool profitable_to_alch = filter.find_profitable_to_alch_items ? (item.price + nature_rune_cost) < item.high_alch : true;

			return generic_filters && name_filter && regex_match && profitable_to_alch;
		});

		return result;
	}
}
