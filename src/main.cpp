#include "DB.hpp"
#include "Types.hpp"

#include <algorithm>
#include <clipp.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

int main(int argc, char** argv)
{
	bool update_db = false;
	bool show_help = false;
	bool pick_random_item = false;

	i64 min_price = 0;
	i64 max_price = std::numeric_limits<i64>::max();
	i64 budget = std::numeric_limits<i64>::max();

	i64 min_volume = 0;
	i64 max_volume = std::numeric_limits<i64>::max();

	i64 min_cost = 0;

	std::string name_contains;

	auto cli = (
		clipp::option("--help", "-h").set(show_help).doc("show help"),
		clipp::option("-u").set(update_db).doc("update the database before processing the query"),
		clipp::option("-r").set(pick_random_item).doc("pick a random item from results"),
		clipp::option("--min-price") & clipp::value("minimum price", min_price),
		clipp::option("--max-price") & clipp::value("maximum price", max_price),
		clipp::option("--min-volume") & clipp::value("minimum volume", min_volume),
		clipp::option("--max-volume") & clipp::value("maximum volume", max_volume),
		clipp::option("--budget", "-b") & clipp::value("budget", budget),
		clipp::option("--name") & clipp::value("name contains", name_contains),
		clipp::option("--min-cost") & clipp::value("minimum cost of the flip", min_cost)
	);

	clipp::parse(argc, argv, cli);

	if (show_help)
	{
		std::cout << clipp::make_man_page(cli, "ge-inspector");
		return 0;
	}

	if (update_db)
		ge::update_db();

	std::vector<ge::item> items = ge::load_db();

	// Run the query
	std::vector<ge::item> filtered_items;
	std::copy_if(items.begin(), items.end(), std::back_inserter(filtered_items), [&](const ge::item& item){
		bool price_filters = item.price >= min_price
	                && item.price <= max_price
				    && (item.price * item.limit) <= budget
					&& (item.price * item.limit) >= min_cost
				    && item.volume >= min_volume
				    && item.volume <= max_volume;

		bool name_filter = name_contains.empty();
		if (!name_contains.empty())
			name_filter = item.name.find(name_contains) != std::string::npos;

		return price_filters && name_filter;
	});

	// Pick a random item from results
	if (pick_random_item)
	{
		srand(time(0));
		ge::item item = filtered_items.at(rand() % filtered_items.size());

		std::cout << "Item:\t\t" << item.name << '\n'
				<< "Price:\t\t" << item.price << '\n'
				<< "Previous price:\t" << item.prev_price << '\n'
				<< "Limit:\t\t" << item.limit << '\n'
				<< "Volume:\t\t" << item.volume << '\n'
				<< "Total cost:\t" << item.limit * item.price << '\n'
				<< "High alch:\t" << item.high_alch << '\n';

	}
	// Print the results normally
	else
	{
		u8 name_width = 42;
		u8 price_width = 10;
		u8 old_price_width = 12;
		u8 volume_width = 10;
		u8 total_cost_width = 12;
		u8 limit_width = 10;
		u8 high_alch_width = 12;

		std::cout << std::left
			<< std::setw(name_width) << "Name"
			<< std::setw(price_width) << "Price"
			<< std::setw(old_price_width) << "Old price"
			<< std::setw(volume_width) << "Volume"
			<< std::setw(total_cost_width) << "Total cost"
			<< std::setw(limit_width) << "Limit"
			<< std::setw(high_alch_width) << "High alch"
			<< std::endl;

		for (const ge::item& item : filtered_items)
			std::cout << std::left
				<< std::setw(name_width) << item.name
				<< std::setw(price_width) << item.price
				<< std::setw(old_price_width) << item.prev_price
				<< std::setw(volume_width) << item.volume
				<< std::setw(total_cost_width) << item.price * item.limit
				<< std::setw(limit_width) << item.limit
				<< std::setw(high_alch_width) << item.high_alch
				<< "\n";
	}

	return 0;
}
