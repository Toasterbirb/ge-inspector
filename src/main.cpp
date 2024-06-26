#include "DB.hpp"
#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <clipp.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <ranges>

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

	bool invert_sort = false;

	ge::sort_mode sort_mode = ge::sort_mode::none;

	auto cli_sort_volume = (
		clipp::command("volume").set(sort_mode, ge::sort_mode::volume).doc("sort by volume")
	);

	auto cli_sort_price = (
		clipp::command("price").set(sort_mode, ge::sort_mode::price).doc("sort by price")
	);

	auto cli_sort_alch = (
		clipp::command("alch").set(sort_mode, ge::sort_mode::alch).doc("sort by the high alchemy price")
	);

	auto cli_sort_cost = (
		clipp::command("cost").set(sort_mode, ge::sort_mode::cost).doc("sort by total cost")
	);

	auto cli_sort_limit = (
		clipp::command("limit").set(sort_mode, ge::sort_mode::limit).doc("sort by buy limit")
	);

	auto cli = (
		clipp::option("--help", "-h").set(show_help).doc("show help"),
		clipp::option("-u").set(update_db).doc("update the database before processing the query"),
		clipp::option("-r").set(pick_random_item).doc("pick a random item from results"),
		clipp::option("--min-price").doc("minimum price") & clipp::value("price", min_price),
		clipp::option("--max-price").doc("maximum price") & clipp::value("price", max_price),
		clipp::option("--min-volume").doc("minimum volume") & clipp::value("volume", min_volume),
		clipp::option("--max-volume").doc("maximum volume") & clipp::value("volume", max_volume),
		clipp::option("--budget", "-b").doc("maximum budget for total cost") & clipp::value("budget", budget),
		clipp::option("--name", "-n").doc("filter items by name") & clipp::value("str", name_contains),
		clipp::option("--min-cost").doc("minimum cost of the flip") & clipp::value("cost", min_cost),
		(clipp::option("--sort", "-s").doc("sort the results") & (cli_sort_volume | cli_sort_price | cli_sort_alch | cli_sort_cost | cli_sort_limit) & clipp::option("--invert", "-i").set(invert_sort).doc("invert the sorting result"))
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
		{
			// Convert everything to lowercase before doing the comparisons
			std::string lowercase_name;

			std::transform(item.name.begin(), item.name.end(), std::back_inserter(lowercase_name), [](unsigned char c) {
				return std::tolower(c);
			});

			std::transform(name_contains.begin(), name_contains.end(), name_contains.begin(), [](unsigned char c) {
				return std::tolower(c);
			});

			name_filter = lowercase_name.find(name_contains) != std::string::npos;
		}

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
		// Sort the filtered list
		if (sort_mode != ge::sort_mode::none)
			ge::sort_items(filtered_items, sort_mode);

		u8 name_width = 42;
		u8 price_width = 10;
		u8 volume_width = 10;
		u8 total_cost_width = 12;
		u8 limit_width = 10;
		u8 high_alch_width = 12;

		std::cout << std::left
			<< std::setw(name_width) << "Name"
			<< std::setw(price_width) << "Price"
			<< std::setw(volume_width) << "Volume"
			<< std::setw(total_cost_width) << "Total cost"
			<< std::setw(limit_width) << "Limit"
			<< std::setw(high_alch_width) << "High alch"
			<< std::endl;

		auto print_item_line = [&](const ge::item& item)
		{
			std::cout << std::left
				<< std::setw(item.name.size() < name_width ? name_width : item.name.size() + 1) << item.name
				<< std::setw(price_width) << item.price
				<< std::setw(volume_width) << item.volume
				<< std::setw(total_cost_width) << item.price * item.limit
				<< std::setw(limit_width) << item.limit
				<< std::setw(high_alch_width) << item.high_alch
				<< "\n";
		};

		if (!invert_sort)
		{
			for (const ge::item& item : filtered_items)
				print_item_line(item);
		}
		else
		{
			for (const ge::item& item : filtered_items | std::views::reverse)
				print_item_line(item);
		}
	}

	return 0;
}
