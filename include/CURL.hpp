#pragma once

#include "Types.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace ge
{
	nlohmann::json download_json(const std::string& url, u8 depth);
}
