#pragma once

#include "DB.hpp"

namespace ge
{
	static auto sort_by_price = [](const item& a, const item& b) {
		return a.price < b.price;
	};

	static auto sort_by_alch = [](const item& a, const item& b) {
		return a.high_alch < b.high_alch;
	};

	static auto sort_by_volume = [](const item& a, const item& b) {
		return a.volume < b.volume;
	};

	static auto sort_by_total_cost = [](const item& a, const item& b) {
		return (a.limit * a.price) < (b.limit * b.price);
	};

	static auto sort_by_limit = [](const item& a, const item& b) {
		return a.limit < b.limit;
	};
}
