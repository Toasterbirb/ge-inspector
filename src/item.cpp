#include "Item.hpp"

#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace ge
{
	void to_json(nlohmann::json& j, const item& i)
	{
		j = nlohmann::json {
			{ "name", i.name },
			{ "id", i.id },
			{ "price", i.price },
			{ "limit", i.limit },
			{ "volume", i.volume },
			{ "high_alch", i.high_alch },
			{ "members", i.members },
			{ "category", i.category },
		};
	}

	void from_json(const nlohmann::json& j, item& i)
	{
		j.at("name").get_to(i.name);
		j.at("id").get_to(i.id);
		j.at("price").get_to(i.price);
		j.at("limit").get_to(i.limit);
		j.at("volume").get_to(i.volume);
		j.at("high_alch").get_to(i.high_alch);
		j.at("members").get_to(i.members);
		j.at("category").get_to(i.category);
	}

	void list_categories()
	{
		// Some duplicates that should be skipped
		static const std::unordered_set<std::string> duplicate_category_names = {
			"Magic armour", "Magic weapons",
			"Ranged armour", "Ranged weapons",
		};

		std::vector<std::pair<std::string, u8>> sorted_category_list;

		std::transform(item_categories.begin(), item_categories.end(), std::back_inserter(sorted_category_list), [](const std::pair<std::string, u8>& category){
			return category;
		});

		std::sort(sorted_category_list.begin(), sorted_category_list.end(), [](const std::pair<std::string, u8>& a, const std::pair<std::string, u8>& b){
			return a.second < b.second;
		});

		for (const auto[name, id] : sorted_category_list)
		{
			// Skip duplicate categories
			if (duplicate_category_names.contains(name))
				continue;

			std::cout << "[" << std::setw(2) << static_cast<i32>(id) << "] " << name << "\n";
		}
	}

	void sort_items(std::vector<item>& items, const sort_mode mode)
	{
		if (mode == sort_mode::none)
			return;

		// Find the matching sorting mode
		auto sort_mode_it = std::find_if(sorting_modes.begin(), sorting_modes.end(), [mode](const auto& sort_mode_tuple){
			return std::get<sort_mode>(sort_mode_tuple) == mode;
		});

		assert(sort_mode_it != sorting_modes.end());
		if (sort_mode_it == sorting_modes.end())
			return;

		// Sort the item list with the sorting lambda
		std::sort(items.begin(), items.end(), std::get<std::function<bool(const item& a, const item& b)>>(*sort_mode_it));
	}
}
