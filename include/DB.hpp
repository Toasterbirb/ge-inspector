#pragma once

#include "Types.hpp"

#include <nlohmann/json_fwd.hpp>

namespace ge
{
	struct item;

	void init_db();
	void update_db();
	void update_item(const item& item);
	void write_db();

	// Returns true if the member status was updated
	bool update_item_member_status(item& item);

	std::vector<item> load_db();

	void update_filtered_item_data(std::vector<item>& items);
	u64 item_cost(const std::string& name);
}
