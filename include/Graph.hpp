#pragma once

#include "Types.hpp"

#include <vector>

namespace ge
{
	double normalize(const u64 value, const u64 min, const u64 max, const u64 normalized_maximum);
	void draw_graph(const i8 height, const std::vector<u64>& values);
}
