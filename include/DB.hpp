#pragma once

#include "Types.hpp"

#include <nlohmann/json.hpp>

namespace ge
{
	struct item
	{
		std::string name;
		i64 price{};
		i32 limit{};
		i32 prev_price{};
		i64 volume{};
		i32 high_alch{};

	};

	enum sort_mode
	{
		volume, price, alch, cost, limit, none
	};

	void to_json(nlohmann::json& j, const item& i);
	void from_json(const nlohmann::json& j, item& i);

	nlohmann::json download_json(const std::string& url);
	void update_db();

	std::vector<item> load_db();

	void sort_items(std::vector<item>& items, const sort_mode mode);
}
