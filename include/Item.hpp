#pragma once

#include "Types.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <tuple>
#include <unordered_map>

namespace ge
{
	enum class members_item
	{
		yes, no, unknown, no_data
	};

	const static std::unordered_map<members_item, std::string> members_item_str = {
		{ members_item::yes, "yes" },
		{ members_item::no, "no" },
		{ members_item::unknown, "unknown" },
		{ members_item::no_data, "unknown" }
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
		u8 category = -1;

		std::vector<u64> price_history;
		u64 last_price_history_update{};
	};

	const static inline std::string all_category = "all";
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
		{ "Necromancy armour", 43 },
		{ all_category, -2 },
		{ "unknown", -1 },
	};

	std::string category_id_to_str(const u8 category_id);
	void list_categories();

	enum class sort_mode
	{
		volume, price, alch, alch_profit, cost, limit, volume_limit_ratio, none
	};

	static inline const std::array<std::tuple<sort_mode, std::string, std::string, std::function<bool(const item& a, const item& b)>>, 7> sorting_modes = {
		std::make_tuple(sort_mode::volume, "volume", "sort by volume",
			[](const item& a, const item& b)
			{
				return a.volume < b.volume;
			}
		),

		std::make_tuple(sort_mode::price, "price", "sort by price",
			[](const item& a, const item& b)
			{
				return a.price < b.price;
			}
		),

		std::make_tuple(sort_mode::alch, "alch", "sort by the high alchemy price",
			[](const item& a, const item& b)
			{
				return a.high_alch < b.high_alch;
			}
		),

		std::make_tuple(sort_mode::alch_profit, "alch_profit", "sort by the profit margin of high alchemy",
			[](const item& a, const item& b)
			{
				return (a.high_alch - a.price) < (b.high_alch - b.price);
			}
		),

		std::make_tuple(sort_mode::cost, "cost", "sort by total cost",
			[](const item& a, const item& b)
			{
				return (a.limit * a.price) < (b.limit * b.price);
			}
		),

		std::make_tuple(sort_mode::limit, "limit", "sort by buy limit",
			[](const item& a, const item& b)
			{
				return a.limit < b.limit;
			}
		),

		std::make_tuple(sort_mode::volume_limit_ratio, "volume_limit_ratio", "sort by the ratio of volume and buy limit",
			[](const item& a, const item& b)
			{
				return (a.volume / static_cast<f64>(a.limit)) < (b.volume / static_cast<f64>(b.limit));
			}
		),
	};

	void to_json(nlohmann::json& j, const item& i);
	void from_json(const nlohmann::json& j, item& i);

	void sort_items(std::vector<item>& items, const sort_mode mode);
}
