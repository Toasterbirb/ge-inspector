#include "CURL.hpp"
#include "DB.hpp"
#include "Item.hpp"
#include "PriceUtils.hpp"

#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/ioctl.h>
#include <term_chart.hpp>
#include <unistd.h>
#include <unordered_set>

namespace ge
{
	void to_json(nlohmann::json& j, const item& i)
	{
		j = nlohmann::json {
			{ "name", i.name },
			{ "id", i.id },
			{ "price", i.price },
			{ "limit", i.limit },
			{ "volume", i.volume },
			{ "high_alch", i.high_alch },
			{ "members", i.members },
			{ "category", i.category },
			{ "price_history", i.price_history },
			{ "prev_price_hist_update", i.last_price_history_update },
			{ "last_random_time", i.last_random_time },
		};
	}

	void from_json(const nlohmann::json& j, item& i)
	{
		j.at("name").get_to(i.name);
		j.at("id").get_to(i.id);
		j.at("price").get_to(i.price);
		j.at("limit").get_to(i.limit);
		j.at("volume").get_to(i.volume);
		j.at("high_alch").get_to(i.high_alch);
		j.at("members").get_to(i.members);
		j.at("category").get_to(i.category);
		j.at("price_history").get_to(i.price_history);
		j.at("prev_price_hist_update").get_to(i.last_price_history_update);

		// for backwards compat reasons, the json might not have this key
		if (j.contains("last_random_time"))
			j.at("last_random_time").get_to(i.last_random_time);
	}

	std::string category_id_to_str(const u8 category_id)
	{
		return std::find_if(item_categories.begin(), item_categories.end(), [category_id](const std::pair<std::string, u8> category)
		{
			return category_id == category.second;
		})->first;
	}

	void list_categories()
	{
		// Some duplicates that should be skipped
		static const std::unordered_set<std::string> duplicate_category_names = {
			"Magic armour", "Magic weapons",
			"Ranged armour", "Ranged weapons",
		};

		std::vector<std::pair<std::string, u8>> sorted_category_list;

		std::transform(item_categories.begin(), item_categories.end(), std::back_inserter(sorted_category_list), [](const std::pair<std::string, u8>& category){
			return category;
		});

		std::sort(sorted_category_list.begin(), sorted_category_list.end(), [](const std::pair<std::string, u8>& a, const std::pair<std::string, u8>& b){
			return a.second < b.second;
		});

		for (const auto& [name, id] : sorted_category_list)
		{
			// Skip duplicate categories
			if (duplicate_category_names.contains(name)) [[unlikely]]
				continue;

			std::cout << "[" << std::setw(2) << static_cast<i32>(id) << "] " << name << "\n";
		}
	}

	void sort_items(std::vector<item>& items, const sort_mode mode)
	{
		if (mode == sort_mode::none)
			return;

		// Find the matching sorting mode
		auto sort_mode_it = std::find_if(sorting_modes.begin(), sorting_modes.end(), [mode](const auto& sort_mode_tuple){
			return std::get<sort_mode>(sort_mode_tuple) == mode;
		});

		assert(sort_mode_it != sorting_modes.end());
		if (sort_mode_it == sorting_modes.end()) [[unlikely]]
			return;

		// Sort the item list with the sorting lambda
		std::sort(items.begin(), items.end(), std::get<std::function<bool(const item& a, const item& b)>>(*sort_mode_it));
	}

	void print_item_info(item& item, const bool print_short_price, const bool print_terse_format, const bool print_price_history, const ge::colorscheme colorscheme)
	{
		constexpr u32 normal_info_column_width = 15;
		const std::string separator = print_terse_format ? ";" : ":";

		// Set the info column width to zero if printing in terse format
		const u32 info_column_width = print_terse_format ? 0 : normal_info_column_width;

		std::cout
				<< std::left
				<< ( colorscheme != ge::colorscheme::white ? "\033[" + ge::next_color(colorscheme) + "m" : "" )
				<< std::setw(info_column_width) << "Item" + separator << item.name << '\n'
				<< std::setw(info_column_width) << "Category" + separator << ge::category_id_to_str(item.category) << '\n'
				<< std::setw(info_column_width) << "Price" + separator << ( print_short_price ? ge::round_big_numbers(item.price) : std::to_string(item.price) ) << '\n'
				<< std::setw(info_column_width) << "Limit" + separator << item.limit << '\n'
				<< std::setw(info_column_width) << "Volume" + separator << ( print_short_price ? ge::round_big_numbers(item.volume) : std::to_string(item.volume) ) << '\n'
				<< std::setw(info_column_width) << "Total cost" + separator << ( print_short_price ? ge::round_big_numbers(item.limit * item.price) : std::to_string(item.limit * item.price) ) << '\n'
				<< std::setw(info_column_width) << "High alch" + separator << item.high_alch << '\n'
				<< std::setw(info_column_width) << "Members" + separator << ge::members_item_str.at(item.members)
				<< ( colorscheme != ge::colorscheme::white ? "\033[0m" : "" )
				<< '\n';


		if (print_price_history)
		{
			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			const u64 day_count = w.ws_col;
			std::vector<u64> price_history = ge::item_price_history(item, day_count);

			const u64 min = *std::min_element(price_history.begin(), price_history.end());
			const u64 max = *std::max_element(price_history.begin(), price_history.end());

			const i64 average = [&price_history]() -> i64
			{
				i64 total = 0;
				for (const i64 price : price_history) total += price;

				return std::round(total / static_cast<f64>(price_history.size()));
			}();

			const auto calc_change = [&price_history](const u8 days) -> f32
			{
				const i64 price_a = *(price_history.end() - days);
				const i64 price_b = *std::prev(price_history.end());

				assert(price_a != 0);
				return (price_b - price_a) / static_cast<f64>(price_a);
			};

			const f32 last_10_day_change = calc_change(10);
			const f32 last_30_day_change = calc_change(30);

			std::cout << "\n - Price history -\n"
				<< std::setw(info_column_width) << "Min" + separator << ( print_short_price ? ge::round_big_numbers(min) : std::to_string(min) ) << '\n'
				<< std::setw(info_column_width) << "Max" + separator << ( print_short_price ? ge::round_big_numbers(max) : std::to_string(max) ) << '\n'
				<< std::setw(info_column_width) << "Average" + separator << ( print_short_price ? ge::round_big_numbers(average) : std::to_string(average) ) << '\n'
				<< std::setw(info_column_width) << "10 day change" + separator << last_10_day_change * 100 << "%\n"
				<< std::setw(info_column_width) << "30 day change" + separator << last_30_day_change * 100 << "%\n";

			constexpr u8 graph_height = 8;
			std::cout << '\n' << term_chart::render(graph_height, price_history);
		}
	}
}
