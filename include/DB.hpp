#pragma once

#include "Types.hpp"

#include <nlohmann/json.hpp>
#include <unordered_map>

namespace ge
{
	enum class members_item
	{
		yes, no, unknown, no_data
	};

	const static std::unordered_map<std::string, u8> item_categories = {
		{ "Miscellaneous", 0 },
		{ "Ammo", 1 },
		{ "Arrows", 2 },
		{ "Bolts", 3 },
		{ "Construction materials", 4 },
		{ "Construction products", 5 },
		{ "Cooking ingredients", 6 },
		{ "Costumes", 7 },
		{ "Crafting materials", 8 },
		{ "Familiars", 9 },
		{ "Farming produce", 10 },
		{ "Fletching materials", 11 },
		{ "Food and Drink", 12 },
		{ "Herblore materials", 13 },
		{ "Hunting equipment", 14 },
		{ "Hunting Produce", 15 },
		{ "Jewellery", 16 },
		{ "Mage armour", 17 },
		{ "Mage weapons", 18 },
		{ "Magic armour", 17 },
		{ "Magic weapons", 18 },
		{ "Melee armour - low level", 19 },
		{ "Melee armour - mid level", 20 },
		{ "Melee armour - high level", 21 },
		{ "Melee weapons - low level", 22 },
		{ "Melee weapons - mid level", 23 },
		{ "Melee weapons - high level", 24 },
		{ "Mining and Smithing", 25 },
		{ "Potions", 26 },
		{ "Prayer armour", 27 },
		{ "Prayer materials", 28 },
		{ "Range armour", 29 },
		{ "Range weapons", 30 },
		{ "Ranged armour", 29 },
		{ "Ranged weapons", 30 },
		{ "Runecrafting", 31 },
		{ "Runes, Spells and Teleports", 32 },
		{ "Seeds", 33 },
		{ "Summoning scrolls", 34 },
		{ "Tools and containers", 35 },
		{ "Woodcutting product", 36 },
		{ "Pocket items", 37 },
		{ "Stone spirits", 38 },
		{ "Salvage", 39 },
		{ "Firemaking products", 40 },
		{ "Archaeology materials", 41 },
		{ "Wood spirits", 42 },
		{ "Necromancy armour", 43 }
	};

	struct item
	{
		std::string name;
		i32 id{};
		i64 price{};
		i32 limit{};
		i64 volume{};
		i32 high_alch{};
		members_item members = members_item::unknown;
	};

	enum class sort_mode
	{
		volume, price, alch, cost, limit, none
	};

	void to_json(nlohmann::json& j, const item& i);
	void from_json(const nlohmann::json& j, item& i);

	nlohmann::json download_json(const std::string& url, u8 depth);
	void init_db();
	void update_db();
	void update_item(const item& item);
	void write_db();

	// Returns true if the member status was updated
	bool update_item_member_status(item& item);

	std::vector<item> load_db();

	void sort_items(std::vector<item>& items, const sort_mode mode);
	void update_filtered_item_data(std::vector<item>& items);
}
