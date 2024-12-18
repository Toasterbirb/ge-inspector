#include "CURL.hpp"
#include "Color.hpp"
#include "DB.hpp"
#include "Filtering.hpp"
#include "Item.hpp"
#include "PriceUtils.hpp"
#include "PrintUtils.hpp"
#include "Random.hpp"
#include "Types.hpp"

#include <chrono>
#include <clipp.h>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sys/ioctl.h>
#include <unistd.h>

using minutes = u32;

int main(int argc, char** argv)
{
	ge::members_item member_filter = ge::members_item::unknown;

	bool update_db = false;
	bool quiet_db_update = false;
	bool show_help = false;
	bool pick_random_item = false;
	bool check_member_status = false;
	bool print_short_price = false;
	bool print_no_header = false;
	bool print_index = false;
	bool print_terse_format = false;
	bool print_price_history = false;
	bool print_category_list = false;
	bool print_count = false;
	minutes min_random_age{0};

	ge::filter filter;

	// Set default range values
	filter.volume.min = 1;

	std::string pre_filter_item_names;

	bool invert_sort = false;
	ge::colorscheme colorscheme = ge::colorscheme::white;

	ge::sort_mode sort_mode = ge::sort_mode::none;

	const clipp::group sorting_mode_commands = [&]() {
		clipp::group cmds;
		for (const auto& [mode, name, description, func] : ge::sorting_modes)
			cmds.push_back(clipp::command(name).set(sort_mode, mode).doc(description));
		return cmds;
	}();

	// Generate the colorscheme option list
	const clipp::group colorscheme_commands = [&]() {
		clipp::group cmds;
		for (const auto& color : ge::colorschemes)
			cmds.push_back(clipp::command(color.second.first).set(colorscheme, color.first));
		return cmds;
	}();

	std::pair<std::string, std::string> ratio_stat_str;
	const std::string ratio_help_str =
		"compare the ratios of different statistics and filter out items that have a ratio of equal or lower than the one stated (true if stat_a >= stat_b * ratio)\n\navailable stats to compare: "
		+ []() -> std::string {
			std::string stats;
			for (const auto& [name, stat] : ge::str_to_stat)
				stats += name + ' ';
			return stats;
		}()
		+ '\n';

	const auto cli = (
		clipp::option("--help", "-h").set(show_help) % "show help",
		(clipp::option("--update", "-u").set(update_db) % "update the database before processing the query"
		 & clipp::option("--quiet", "-q").set(quiet_db_update) % "only update the db and don't print out any results"),
		clipp::option("--list-categories").set(print_category_list) % "list all of the item categories",
		clipp::option("--member", "-m").set(check_member_status) % "update missing members data",
		(clipp::option("--random", "-r").set(pick_random_item) % "pick a random item from results"
		 & (clipp::option("--age", "-a") & clipp::number("minutes").set(min_random_age)) % "only show random flips that haven't been shown in x-amount of minutes"),
		clipp::option("--terse", "-t").set(print_terse_format) % "print the item info in a way that is easier to parse with 3rd party programs (works only if printing info for a singular item)",
		clipp::option("--history").set(print_price_history) % "print price history the item (works only if printing info for a singular item)",
		clipp::option("--count").set(print_count) % "show result count at the end of the output",
		clipp::option("--f2p", "-f").set(member_filter, ge::members_item::no) % "look for f2p items",
		clipp::option("--p2p", "-p").set(member_filter, ge::members_item::yes) % "look for p2p items",
		clipp::option("--profitable-alch").set(filter.find_profitable_to_alch_items) % "find items that are profitable to alch with high alchemy",
		clipp::option("--volume-over-limit").set(filter.volume_over_limit) % "find items with trade volume higher than their buy limit",
		(clipp::option("--stat-ratio")
		 & clipp::value("stat_a", ratio_stat_str.first)
		 & clipp::value("stat_b", ratio_stat_str.second)
		 & clipp::number("ratio", filter.stat_ratio)) % ratio_help_str,
		clipp::option("--short").set(print_short_price) % "print prices in a shorter form eg. 1.2m, 538k",
		clipp::option("--no-header").set(print_no_header) % "don't print the header row",
		clipp::option("--index").set(print_index) % "print the indices of items",
		(clipp::option("--name", "-n") & clipp::value("str", filter.name_contains)).repeatable(true) % "filter items by name",
		(clipp::option("--regex") & clipp::value("pattern", filter.regex_patterns)).repeatable(true) % "filter items by name with regex",
		((clipp::option("--pre-filter") & clipp::value("items", pre_filter_item_names)) % "set base values for price, volume and limit based on different items\n\nthe item names should be given as a semicolon separated list like this 'Iron ore;Adamant bar;Feathers'"
			& (clipp::option("--fuzz") & clipp::number("fuzz_factor").set(filter.pre_filter_fuzz_factor)) % "allow some variance in the pre-filter value checks (def: 0.05)"),
		(clipp::option("--category", "-c") & clipp::number("category", filter.category)) % "filter items by category (see categories with --list-categories)",
		(clipp::option("--min-price") & clipp::number("price", filter.price.min)) % "minimum price",
		(clipp::option("--max-price") & clipp::number("price", filter.price.max)) % "maximum price",
		(clipp::option("--min-volume") & clipp::number("volume", filter.volume.min)) % "minimum volume (def: 1)",
		(clipp::option("--max-volume") & clipp::number("volume", filter.volume.max)) % "maximum volume",
		(clipp::option("--min-limit") & clipp::number("limit", filter.limit.min)) % "minimum buy limit",
		(clipp::option("--max-limit") & clipp::number("limit", filter.limit.max)) % "maximum buy limit",
		(clipp::option("--min-alch") & clipp::number("alch", filter.alch.min)) % "minimum high alchemy amount",
		(clipp::option("--max-alch") & clipp::number("alch", filter.alch.max)) % "maximum high alchemy amount",
		(clipp::option("--min-cost") & clipp::number("cost", filter.cost.min)) % "minimum cost of the flip",
		(clipp::option("--min-profit") & clipp::number("price-change-%", filter.min_margin_percent) & clipp::number("profit_goal", filter.min_margin_profit_goal)) % "find items where the given price increase (in percent, eg. 2 == 2% and 0.5 == 0.5%) would result in the profit goal assuming the full buy-limit can be bought",
		(clipp::option("--budget", "-b", "--max-cost") & clipp::number("budget", filter.cost.max)) % "maximum budget for total cost",
		(clipp::option("--sort", "-s") & clipp::one_of(sorting_mode_commands)) % "sort the results",
		clipp::option("--invert", "-i").set(invert_sort) % "invert the result order",
		(clipp::option("--color") & clipp::one_of(colorscheme_commands)) % "change the colorscheme of the output to something other than white"
	);

	if (!clipp::parse(argc, argv, cli)) [[unlikely]]
	{
		std::cout << "## Missing or invalid arguments ##\n\n"
			<< "Usage: ge-inspector [options...]\n"
			<< clipp::usage_lines(cli) << "\n\n"
			<< "For more information check the output of 'ge-inspector --help'\n";

		return 1;
	}

	if (show_help) [[unlikely]]
	{
		const auto fmt = clipp::doc_formatting{}.doc_column(40);

		std::cout << clipp::make_man_page(cli, "ge-inspector", fmt);
		return 0;
	}

	if (update_db) [[unlikely]]
		ge::update_db();

	// Quit early if only a DB update was requested
	if (quiet_db_update) [[unlikely]]
		return 0;

	if (print_category_list)
	{
		ge::list_categories();
		return 0;
	}

	// Process ratio stat names
	if (!ratio_stat_str.first.empty() && !ratio_stat_str.second.empty())
	{
		constexpr char check_help_str[] = "check 'flip help' for more information";

		// Check if the names are valid
		if (!ge::str_to_stat.contains(ratio_stat_str.first))
		{
			std::cout << "stat_a is invalid\n" << check_help_str << "\n";
			return 1;
		}

		if (!ge::str_to_stat.contains(ratio_stat_str.second))
		{
			std::cout << "stat_b is invalid\n" << check_help_str << "\n";
			return 1;
		}

		filter.ratio_stat_a = ge::str_to_stat.at(ratio_stat_str.first);
		filter.ratio_stat_b = ge::str_to_stat.at(ratio_stat_str.second);
	}

	std::vector<ge::item> items = ge::load_db();

	// Set values based on pre-filtering
	// However don't change user-defined values
	if (!pre_filter_item_names.empty())
		filter = ge::pre_filter(filter, items, pre_filter_item_names);

	// Run the query
	std::vector<ge::item> filtered_items = ge::filter_items(items, filter);

	// Pick a random item from results
	if (pick_random_item)
	{
		if (filtered_items.empty()) [[unlikely]]
		{
			std::cout << "No results were found. Please try another query\n";
			return 1;
		}

		srand(ge::random_seed());

		ge::item item = filtered_items.at(rand() % filtered_items.size());

		const static auto now = std::chrono::system_clock::now();
		const auto random_item_filter = [&](ge::item& item) -> bool
		{
			// Check if the item is a members item
			if (check_member_status && ge::update_item_member_status(item)) [[unlikely]]
			{
				ge::update_item(item);
				ge::update_filtered_item_data(filtered_items);
			}

			const bool member_check = member_filter == ge::members_item::unknown
				? true // we are not filtering by member status
				: item.members == member_filter;

			const auto age = std::chrono::system_clock::time_point{ std::chrono::nanoseconds{ item.last_random_time } };
			const auto duration = std::chrono::duration(now - age);

			const bool age_check = min_random_age == 0
				? true // only check the age if the minimum is > 0
				: std::chrono::duration_cast<std::chrono::minutes>(duration).count() >= min_random_age;

			return member_check && age_check;
		};

		u64 checked_item_count = 0;
		constexpr u64 checked_items_hard_cap = 64;
		const u64 max_checked_items = filtered_items.size() < checked_items_hard_cap ? filtered_items.size() : checked_items_hard_cap;

		std::cout << "\rItems checked: 0/" << max_checked_items << std::flush;
		while (!random_item_filter(item) && checked_item_count < max_checked_items)
		{
			item = filtered_items.at(rand() % filtered_items.size());
			++checked_item_count;
			std::cout << "\rItems checked: " << checked_item_count << "/" << max_checked_items << std::flush;
		}
		ge::clear_current_line();

		if (checked_item_count >= max_checked_items) [[unlikely]]
			std::cout << "Reached the maximum amount of items to check. Try again with different search options\n";

		ge::print_item_info(item, print_short_price, print_terse_format, print_price_history, colorscheme);

		// Update the item check age
		item.last_random_time = now.time_since_epoch().count();
		ge::update_item(item);
		ge::write_db();
	}

	// Print the results normally if there are any results to print
	if (!pick_random_item && filtered_items.size() > 1) [[likely]]
	{
		// Sort the filtered list
		if (sort_mode != ge::sort_mode::none)
			ge::sort_items(filtered_items, sort_mode);

		constexpr u8 index_width = 6;

		// Name width based on the longest name
		constexpr u8 padding = 2;
		const u8 name_width = [&filtered_items]() -> u8 {
			std::vector<u8> name_lengths(filtered_items.size());
			std::transform(filtered_items.begin(), filtered_items.end(), name_lengths.begin(), [](const ge::item& item) {
				return item.name.size();
			});
			return *std::max_element(name_lengths.begin(), name_lengths.end()) + padding;
		}();

		constexpr u8 price_width = 12;
		constexpr u8 volume_width = 10;
		constexpr u8 total_cost_width = 13;
		constexpr u8 limit_width = 10;
		constexpr u8 high_alch_width = 12;
		constexpr u8 members_width = 10;

		if (!print_no_header) [[unlikely]]
		{
			std::cout << std::left
				<< std::setw(print_index ? index_width : 0) << ( print_index ? "Index" : "" )
				<< std::setw(name_width) << "Name"
				<< std::setw(price_width) << "Price"
				<< std::setw(volume_width) << "Volume"
				<< std::setw(limit_width) << "Limit"
				<< std::setw(total_cost_width) << "Total cost"
				<< std::setw(high_alch_width) << "High alch"
				<< std::setw(members_width) << "Members"
				<< "\n";
		}

		u16 index = 0;
		const auto print_item_line = [=, &filtered_items, &index](ge::item& item)
		{
			if (check_member_status && ge::update_item_member_status(item)) [[unlikely]]
				ge::update_filtered_item_data(filtered_items);

			// Handle members status filters
			if (member_filter != ge::members_item::unknown && item.members != member_filter)
				return;

			const u64 total_item_cost = item.price * item.limit;

			std::cout
				<< ( colorscheme != ge::colorscheme::white ? "\033[" + ge::next_color(colorscheme) + "m" : "" )
				<< std::setw(print_index ? index_width : 0) << ( print_index ? std::to_string(index) : "" )
				<< std::setw(item.name.size() < name_width ? name_width : item.name.size() + 1) << item.name
				<< std::setw(price_width) << ( print_short_price ? ge::round_big_numbers(item.price) : std::to_string(item.price) )
				<< std::setw(volume_width) << ( print_short_price ? ge::round_big_numbers(item.volume) : std::to_string(item.volume) )
				<< std::setw(limit_width) << item.limit
				<< std::setw(total_cost_width) << ( print_short_price ? ge::round_big_numbers(total_item_cost) : std::to_string(total_item_cost) )
				<< std::setw(high_alch_width) << item.high_alch
				<< std::setw(members_width) << ge::members_item_str.at(item.members)
				<< ( colorscheme != ge::colorscheme::white ? "\033[0m" : "" )
				<< '\n';

			++index;
		};

		std::cout << std::left;

		if (!invert_sort)
			for (ge::item& item : filtered_items)
				print_item_line(item);
		else
			for (ge::item& item : filtered_items | std::views::reverse)
				print_item_line(item);
	}
	else if (filtered_items.size() == 1) [[unlikely]]
	{
		ge::item& item = filtered_items.at(0);

		if (item.members == ge::members_item::unknown && check_member_status) [[unlikely]]
			ge::update_item_member_status(item);

		// Print the information for the only item in the list
		ge::print_item_info(item, print_short_price, print_terse_format, print_price_history, colorscheme);
	}

	if (print_count)
		std::cout << (colorscheme != ge::colorscheme::white ? "\033[" + ge::next_color(colorscheme) + "m" : "")
			<< "\nResults: " << filtered_items.size() << '\n';

	return 0;
}
