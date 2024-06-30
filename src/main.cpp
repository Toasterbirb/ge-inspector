#include "Color.hpp"
#include "DB.hpp"
#include "Item.hpp"
#include "PriceUtils.hpp"
#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <clipp.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ranges>
#include <regex>

std::vector<std::string> tokenize_string(const std::string& line, const char separator);

int main(int argc, char** argv)
{
	struct range
	{
		i64 min = 0;
		i64 max = std::numeric_limits<i64>::max();

		__attribute__((warn_unused_result))
		bool is_min_set() const { return min > 1; }
		bool is_max_set() const { return max < std::numeric_limits<i64>::max(); }
	};

	ge::members_item member_filter = ge::members_item::unknown;

	bool update_db = false;
	bool show_help = false;
	bool pick_random_item = false;
	bool check_member_status = false;
	bool find_profitable_to_alch_items = false;
	bool print_short_price = false;
	bool print_no_header = false;
	bool print_index = false;
	std::string regex_pattern;

	// Filtering ranges
	range price, volume, limit, alch, cost;

	// Set default range values
	volume.min = 1;

	std::string name_contains;
	std::string pre_filter_item_names;

	bool invert_sort = false;
	ge::colorscheme colorscheme = ge::colorscheme::white;

	ge::sort_mode sort_mode = ge::sort_mode::none;

	clipp::group sorting_mode_commands;
	for (const auto& [mode, name, description] : ge::sorting_modes)
		sorting_mode_commands.push_back(clipp::command(name).set(sort_mode, mode).doc(description));

	// Generate the colorscheme option list
	clipp::group colorscheme_commands;
	for (const auto& color : ge::colorschemes)
		colorscheme_commands.push_back(clipp::command(color.second.first).set(colorscheme, color.first));

	auto cli = (
		clipp::option("--help", "-h").set(show_help) % "show help",
		clipp::option("--update", "-u").set(update_db) % "update the database before processing the query",
		clipp::option("--member", "-m").set(check_member_status) % "update missing members data",
		clipp::option("--random", "-r").set(pick_random_item) % "pick a random item from results",
		clipp::option("--f2p", "-f").set(member_filter, ge::members_item::no) % "look for f2p items",
		clipp::option("--p2p", "-p").set(member_filter, ge::members_item::yes) % "look for p2p items",
		clipp::option("--profitable-alch").set(find_profitable_to_alch_items) % "find items that are profitable to alch with high alchemy",
		clipp::option("--short").set(print_short_price) % "print prices in a shorter form eg. 1.2m, 538k",
		clipp::option("--no-header").set(print_no_header) % "don't print the header row",
		clipp::option("--index").set(print_index) % "print the indices of items",
		(clipp::option("--name", "-n") & clipp::value("str", name_contains)) % "filter items by name",
		(clipp::option("--regex") & clipp::value("pattern", regex_pattern)) % "filter items by name with regex",
		(clipp::option("--pre-filter") & clipp::value("items", pre_filter_item_names)) % "set base values for price, volume and limit based on different items\n\nthe item names should be given as a semicolon separated list like this 'Iron ore;Adamant bar;Feathers'",
		(clipp::option("--min-price") & clipp::value("price", price.min)) % "minimum price",
		(clipp::option("--max-price") & clipp::value("price", price.max)) % "maximum price",
		(clipp::option("--min-volume") & clipp::value("volume", volume.min)) % "minimum volume (def: 1)",
		(clipp::option("--max-volume") & clipp::value("volume", volume.max)) % "maximum volume",
		(clipp::option("--min-limit") & clipp::value("limit", limit.min)) % "minimum buy limit",
		(clipp::option("--max-limit") & clipp::value("limit", limit.max)) % "maximum buy limit",
		(clipp::option("--min-alch") & clipp::value("alch", alch.min)) % "minimum high alchemy amount",
		(clipp::option("--max-alch") & clipp::value("alch", alch.max)) % "maximum high alchemy amount",
		(clipp::option("--min-cost") & clipp::value("cost", cost.min)) % "minimum cost of the flip",
		(clipp::option("--budget", "-b", "--max-cost") & clipp::value("budget", cost.max)) % "maximum budget for total cost",
		(clipp::option("--sort", "-s") & clipp::one_of(sorting_mode_commands)) % "sort the results",
		clipp::option("--invert", "-i").set(invert_sort) % "invert the result order",
		(clipp::option("--color", "-c") & clipp::one_of(colorscheme_commands)) % "change the colorscheme of the output to something other than white"
	);

	if (!clipp::parse(argc, argv, cli))
	{
		std::cout << "## Missing or invalid arguments ##\n\n"
			<< "Usage: ge-inspector [options...]\n"
			<< clipp::usage_lines(cli) << "\n\n"
			<< "For more information check the output of 'ge-inspector --help'\n";

		return 1;
	}

	if (show_help)
	{
		auto fmt = clipp::doc_formatting{}.doc_column(40);

		std::cout << clipp::make_man_page(cli, "ge-inspector", fmt);
		return 0;
	}

	if (update_db)
		ge::update_db();

	std::vector<ge::item> items = ge::load_db();

	// Check the cost of a nature rune (we are assuming that a fire battlestaff is used)
	// This variable is only really used if we are checking for alching profitability
	u64 nature_rune_cost = find_profitable_to_alch_items ? ge::item_cost("Nature rune") : 0;

	// Set values based on pre-filtering
	// However don't change user-defined values
	{
		std::vector<std::string> item_names = tokenize_string(pre_filter_item_names, ';');

		// Determine the min and max values from the given items
		std::vector<ge::item> pre_filtered_items;
		for (const std::string& pre_filter_item_name : item_names)
		{
			std::copy_if(items.begin(), items.end(), std::back_inserter(pre_filtered_items), [pre_filter_item_name](const ge::item& item){
				return pre_filter_item_name == item.name;
			});
		}

		range pre_price { std::numeric_limits<i64>::max(), 0 };
		range pre_volume { std::numeric_limits<i64>::max(), 0 };
		range pre_limit { std::numeric_limits<i64>::max(), 0 };

		const auto set_min_max_to_range = [](range& range, const i64& value)
		{
			if (value < range.min)
				range.min = value;

			if (value > range.max)
				range.max = value;
		};

		for (const ge::item& item : pre_filtered_items)
		{
			set_min_max_to_range(pre_price, item.price);
			set_min_max_to_range(pre_volume, item.volume);
			set_min_max_to_range(pre_limit, item.limit);
		}

		const auto apply_ranges_not_changed_by_user = [](const range src, range& dst)
		{
			dst.min = dst.is_min_set() ? dst.min : src.min;
			dst.max = dst.is_max_set() ? dst.max : src.max;
		};

		apply_ranges_not_changed_by_user(pre_price, price);
		apply_ranges_not_changed_by_user(pre_volume, volume);
		apply_ranges_not_changed_by_user(pre_limit, limit);
	}

	// Run the query
	std::vector<ge::item> filtered_items;
	std::copy_if(items.begin(), items.end(), std::back_inserter(filtered_items), [&](const ge::item& item){
		bool generic_filters = item.price >= price.min
					&& item.price <= price.max
					&& (item.price * item.limit) <= cost.max
					&& (item.price * item.limit) >= cost.min
					&& item.volume >= volume.min
					&& item.volume <= volume.max
					&& item.limit >= limit.min
					&& item.limit <= limit.max
					&& item.high_alch >= alch.min
					&& item.high_alch <= alch.max;

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

		bool regex_match = regex_pattern.empty() ? true : std::regex_match(item.name, std::regex(regex_pattern));
		bool profitable_to_alch = find_profitable_to_alch_items ? (item.price + nature_rune_cost) < item.high_alch : true;

		return generic_filters && name_filter && regex_match && profitable_to_alch;
	});

	// Pick a random item from results
	if (pick_random_item)
	{
		if (filtered_items.empty())
		{
			std::cout << "No results were found. Please try another query\n";
			return 1;
		}

		size_t seed;
		std::fstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
		urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
		srand(seed);

		ge::item item = filtered_items.at(rand() % filtered_items.size());

		// If a members item filter is used, loop until an item is found that matches the filter
		if (member_filter != ge::members_item::unknown)
		{
			u64 checked_item_count = 0;
			constexpr u64 checked_items_hard_cap = 64;
			const u64 max_checked_items = filtered_items.size() < checked_items_hard_cap ? filtered_items.size() : checked_items_hard_cap;

			std::cout << "\rItems checked: 0/" << max_checked_items << std::flush;
			while (item.members != member_filter && checked_item_count < max_checked_items)
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

		if (item.members == ge::members_item::unknown && check_member_status)
			ge::update_item_member_status(item);

		std::cout
				<< ( colorscheme != ge::colorscheme::white ? "\033[" + ge::next_color(colorscheme) + "m" : "" )
				<< "Item:\t\t" << item.name << '\n'
				<< "Price:\t\t" << ( print_short_price ? ge::round_big_numbers(item.price) : std::to_string(item.price) ) << '\n'
				<< "Limit:\t\t" << item.limit << '\n'
				<< "Volume:\t\t" << item.volume << '\n'
				<< "Total cost:\t" << ( print_short_price ? ge::round_big_numbers(item.limit * item.price) : std::to_string(item.limit * item.price) ) << '\n'
				<< "High alch:\t" << item.high_alch << '\n'
				<< "Members:\t" << ge::members_item_str.at(item.members)
				<< ( colorscheme != ge::colorscheme::white ? "\033[0m" : "" )
				<< '\n';
	}
	// Print the results normally
	else
	{
		// Sort the filtered list
		if (sort_mode != ge::sort_mode::none)
			ge::sort_items(filtered_items, sort_mode);

		constexpr u8 index_width = 6;
		constexpr u8 name_width = 42;
		constexpr u8 price_width = 12;
		constexpr u8 volume_width = 10;
		constexpr u8 total_cost_width = 13;
		constexpr u8 limit_width = 10;
		constexpr u8 high_alch_width = 12;
		constexpr u8 members_width = 10;

		if (!print_no_header)
		{
			std::cout << std::left
				<< std::setw(print_index ? index_width : 0) << ( print_index ? "Index" : "" )
				<< std::setw(name_width) << "Name"
				<< std::setw(price_width) << "Price"
				<< std::setw(volume_width) << "Volume"
				<< std::setw(total_cost_width) << "Total cost"
				<< std::setw(limit_width) << "Limit"
				<< std::setw(high_alch_width) << "High alch"
				<< std::setw(members_width) << "Members"
				<< "\n";
		}

		u16 index = 0;
		auto print_item_line = [&](ge::item& item)
		{
			if (check_member_status && ge::update_item_member_status(item))
				ge::update_filtered_item_data(filtered_items);

			// Handle members status filters
			if (member_filter != ge::members_item::unknown && item.members != member_filter)
				return;

			const u64 total_item_cost = item.price * item.limit;

			std::cout
				<< ( colorscheme != ge::colorscheme::white ? "\033[" + ge::next_color(colorscheme) + "m" : "" )
				<< std::left
				<< std::setw(print_index ? index_width : 0) << ( print_index ? std::to_string(index) : "" )
				<< std::setw(item.name.size() < name_width ? name_width : item.name.size() + 1) << item.name
				<< std::setw(price_width) << ( print_short_price ? ge::round_big_numbers(item.price) : std::to_string(item.price) )
				<< std::setw(volume_width) << item.volume
				<< std::setw(total_cost_width) << ( print_short_price ? ge::round_big_numbers(total_item_cost) : std::to_string(total_item_cost) )
				<< std::setw(limit_width) << item.limit
				<< std::setw(high_alch_width) << item.high_alch
				<< std::setw(members_width) << ge::members_item_str.at(item.members)
				<< ( colorscheme != ge::colorscheme::white ? "\033[0m" : "" )
				<< '\n';

			++index;
		};

		if (!invert_sort)
			for (ge::item& item : filtered_items)
				print_item_line(item);
		else
			for (ge::item& item : filtered_items | std::views::reverse)
				print_item_line(item);
	}

	return 0;
}

// Function stolen from subst
std::vector<std::string> tokenize_string(const std::string& line, const char separator)
{
	std::vector<std::string> tokens;

	std::istringstream line_stream(line);
	std::string token;

	while (std::getline(line_stream, token, separator))
		tokens.push_back(token);

	return tokens;
}
