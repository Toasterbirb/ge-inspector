#pragma once

#include <nlohmann/json.hpp>

namespace ge
{
	struct api_bulk_data
	{
		api_bulk_data();

		nlohmann::json price, id, limit, volume, high_alch;
	};
}
