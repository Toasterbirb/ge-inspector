#include "DB.hpp"
#include "Item.hpp"
#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <clipp.h>
#include <ctime>
#include <execution>
#include <fstream>
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
	bool check_member_status = false;
	bool find_f2p_items = false;
	bool find_profitable_to_alch_items = false;

	i64 min_price = 0;
	i64 max_price = std::numeric_limits<i64>::max();
	i64 budget = std::numeric_limits<i64>::max();

	i64 min_volume = 1;
	i64 max_volume = std::numeric_limits<i64>::max();

	i64 min_limit = 0;
	i64 max_limit = std::numeric_limits<i64>::max();

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
		clipp::option("-m").set(check_member_status).doc("update missing members data"),
		clipp::option("-r").set(pick_random_item).doc("pick a random item from results"),
		clipp::option("--f2p").set(find_f2p_items).doc("look for f2p items"),
		clipp::option("--min-price").doc("minimum price") & clipp::value("price", min_price),
		clipp::option("--max-price").doc("maximum price") & clipp::value("price", max_price),
		clipp::option("--min-volume").doc("minimum volume (def: 1)") & clipp::value("volume", min_volume),
		clipp::option("--max-volume").doc("maximum volume") & clipp::value("volume", max_volume),
		clipp::option("--min-limit").doc("minimum buy limit") & clipp::value("limit", min_limit),
		clipp::option("--max-limit").doc("maximum buy limit") & clipp::value("limit", max_limit),
		clipp::option("--profitable-alch").set(find_profitable_to_alch_items).doc("find items that are profitable to alch with high alchemy"),
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

	// Check the cost of a nature rune (we are assuming that a fire battlestaff is used)
	// This variable is only really used if we are checking for alching profitability
	u64 nature_rune_cost = ge::item_cost("Nature rune");

	// Run the query
	std::vector<ge::item> filtered_items;
	std::copy_if(std::execution::par, items.begin(), items.end(), std::back_inserter(filtered_items), [&](const ge::item& item){
		bool generic_filters = item.price >= min_price
					&& item.price <= max_price
					&& (item.price * item.limit) <= budget
					&& (item.price * item.limit) >= min_cost
					&& item.volume >= min_volume
					&& item.volume <= max_volume
					&& item.limit >= min_limit
					&& item.limit <= max_limit;

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

		bool profitable_to_alch = find_profitable_to_alch_items ? (item.price + nature_rune_cost) < item.high_alch : true;

		return generic_filters && name_filter && profitable_to_alch;
	});

	// Pick a random item from results
	if (pick_random_item)
	{
		size_t seed;
		std::fstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
		urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
		srand(seed);

		ge::item item = filtered_items.at(rand() % filtered_items.size());

		// If find_random_f2p_item is true, loop until a f2p item is found
		if (find_f2p_items)
		{
			u64 checked_item_count = 0;
			constexpr u64 checked_items_hard_cap = 64;
			const u64 max_checked_items = filtered_items.size() < checked_items_hard_cap ? filtered_items.size() : checked_items_hard_cap;

			std::cout << "\rItems checked: 0/" << max_checked_items << std::flush;

			while (item.members != ge::members_item::no && checked_item_count < max_checked_items)
			{
				item = filtered_items.at(rand() % filtered_items.size());

				// Check if the item is a members item
				if (ge::update_item_member_status(item))
				{
					ge::update_item(item);
					ge::update_filtered_item_data(filtered_items);
				}

				++checked_item_count;
				std::cout << "\rItems checked: " << checked_item_count << "/" << max_checked_items << std::flush;
			}
			std::cout << "\33[2K\r";

			if (checked_item_count >= max_checked_items)
				std::cout << "Reached the maximum amount of items to check. Try again with different search options\n";
		}

		std::cout << "Item:\t\t" << item.name << '\n'
				<< "Price:\t\t" << item.price << '\n'
				<< "Limit:\t\t" << item.limit << '\n'
				<< "Volume:\t\t" << item.volume << '\n'
				<< "Total cost:\t" << item.limit * item.price << '\n'
				<< "High alch:\t" << item.high_alch << '\n';

		if (item.members == ge::members_item::unknown && check_member_status)
			ge::update_item_member_status(item);

		std::cout << "Members:\t";
		switch (item.members)
		{
			case ge::members_item::yes:
				std::cout << "yes\n";
				break;

			case ge::members_item::no:
				std::cout << "no\n";
				break;

			case ge::members_item::no_data:
			case ge::members_item::unknown:
				std::cout << "unknown\n";
				break;
		}
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
		u8 members_width = 10;

		std::cout << std::left
			<< std::setw(name_width) << "Name"
			<< std::setw(price_width) << "Price"
			<< std::setw(volume_width) << "Volume"
			<< std::setw(total_cost_width) << "Total cost"
			<< std::setw(limit_width) << "Limit"
			<< std::setw(high_alch_width) << "High alch"
			<< std::setw(members_width) << "Members";

		std::cout << std::endl;

		auto print_item_line = [&](ge::item& item)
		{
			if (check_member_status)
			{
				if (ge::update_item_member_status(item))
					ge::update_filtered_item_data(filtered_items);
			}

			// If the f2p filter is enabled, skip members items
			if (find_f2p_items && item.members != ge::members_item::no)
				return;

			std::cout << std::left
				<< std::setw(item.name.size() < name_width ? name_width : item.name.size() + 1) << item.name
				<< std::setw(price_width) << item.price
				<< std::setw(volume_width) << item.volume
				<< std::setw(total_cost_width) << item.price * item.limit
				<< std::setw(limit_width) << item.limit
				<< std::setw(high_alch_width) << item.high_alch;

			std::cout << std::setw(members_width);
			switch (item.members)
			{
				case ge::members_item::yes:
					std::cout << "yes";
					break;

				case ge::members_item::no:
					std::cout << "no";
					break;

				case ge::members_item::no_data:
				case ge::members_item::unknown:
					std::cout << "unknown";
					break;
			}

			std::cout << "\n";
		};

		if (!invert_sort)
		{
			for (size_t i = 0; i < filtered_items.size(); ++i)
				print_item_line(filtered_items[i]);
		}
		else
		{
			for (ge::item& item : filtered_items | std::views::reverse)
				print_item_line(item);
		}
	}

	return 0;
}
