#include "APIBulkData.hpp"
#include "CURL.hpp"
#include "DB.hpp"
#include "Item.hpp"
#include "PrintUtils.hpp"
#include "Random.hpp"

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace ge
{
	static nlohmann::json DATABASE;

	static const std::string db_dir_path = std::string(getenv("HOME")) + "/.local/share/ge-inspector";
	static const std::string db_path = db_dir_path + "/price.json";

	constexpr char db_update_lock_file[] = "/tmp/ge-inspector-update-lock";

	using namespace std::chrono_literals;
	constexpr std::chrono::duration update_cooldown_time = 1h;

	constexpr char item_details_json_url[] = "https://services.runescape.com/m=itemdb_rs/api/catalogue/detail.json?item=";

	void init_db()
	{
		api_bulk_data api_data;

		nlohmann::json db;

		// Process the data
		{
			for (auto i = api_data.price.begin(); i != api_data.price.end(); ++i)
			{
				// if there is no limit information for the item, skip it entirely
				if (!api_data.limit.contains(i.key()))
					continue;

				// if the item is missing its ID, use the hash of the item name as the ID
				const i32 item_id = api_data.id.contains(i.key())
					? static_cast<i32>(api_data.id[i.key()])
					: static_cast<i32>(std::hash<std::string>{}(i.key()));

				item item {
					i.key(),
					api_data.id[i.key()],
					i.value(),
					api_data.limit[i.key()]
				};

				if (api_data.volume.contains(i.key()))
					item.volume = api_data.volume.at(i.key());

				if (api_data.high_alch.contains(i.key()))
					item.high_alch = api_data.high_alch.at(i.key());

				db["items"].push_back(item);
			}
		}

		// Make sure, that the database directory exists
		std::filesystem::create_directories(db_dir_path);

		// Write the db to file
		std::fstream file(db_path, std::ios::out);
		file << db;
	}

	void update_db()
	{
		// If the database file doesn't exist, initialize it
		// instead of updating the database
		if (!std::filesystem::exists(db_path))
		{
			init_db();
			return;
		}

		// Don't do anything if the database was updated very recently
		if (std::filesystem::exists(db_update_lock_file))
		{
			const std::chrono::time_point db_last_edit = std::filesystem::last_write_time(db_path);
			const std::chrono::time_point now = std::chrono::clock_cast<std::chrono::file_clock>(std::chrono::system_clock::now());

			const auto duration = std::chrono::duration(now - db_last_edit);
			if (std::chrono::duration_cast<std::chrono::hours>(duration) < update_cooldown_time)
				return;
		}

		// Download the latest information from the API
		api_bulk_data api_data;

		nlohmann::json db;

		// Update the database data
		{
			std::ifstream db_file(db_path);
			db = nlohmann::json::parse(db_file);

			assert(!db["items"].empty());
			for (size_t i = 0; i < db["items"].size(); ++i)
			{
				const std::string& item_name = db["items"][i]["name"];

				// If an item name has changed or is invalid, remove the item
				if (!api_data.price.contains(item_name))
				{
					db["items"].erase(i);
					--i;
					continue;
				}

				assert(api_data.price.contains(item_name));

				db["items"][i]["price"] = api_data.price[item_name];

				if (api_data.volume.contains(item_name))
					db["items"][i]["volume"] = api_data.volume.at(item_name);
				else
					db["items"][i]["volume"] = 0;

				if (api_data.high_alch.contains(item_name))
					db["items"][i]["high_alch"] = api_data.high_alch.at(item_name);

				// Erase the item from the price data so that there's less
				// looping to do when handling new items in the next loop
				api_data.price.erase(item_name);
			}

			const bool new_items_exist = !api_data.price.empty();

			// Handle new and/or missing items
			for (auto it = api_data.price.begin(); it != api_data.price.end(); ++it)
			{
				// skip items that don't have limit information (the same as in init_db)
				// if the item doesn't have limit info, its probably missing some other stuff
				// aswell and keeping track of it is a waste of time
				if (!api_data.limit.contains(it.key()))
					continue;

				item item {
					it.key(),
					api_data.id[it.key()],
					it.value(),
					api_data.limit[it.key()]
				};

				if (api_data.volume.contains(it.key()))
					item.volume = api_data.volume.at(it.key());

				if (api_data.high_alch.contains(it.key()))
					item.high_alch = api_data.high_alch.at(it.key());

				db["items"].push_back(item);
			}

			// If there were new items found, sort the items alphabetically to keep the order
			if (new_items_exist)
			{
				std::sort(db["items"].begin(), db["items"].end(),
					[](const nlohmann::json& item_a, const nlohmann::json& item_b)
					{
						const std::string& item_a_name = item_a["name"];
						const std::string& item_b_name = item_b["name"];

						for (size_t i = 0; i < item_a_name.size() && i < item_b_name.size(); ++i)
						{
							if (item_a_name.at(i) == item_b_name.at(i))
								continue;

							return item_a_name.at(i) < item_b_name.at(i);
						}

						return true;
					});
			}
		}

		// Update the database file
		{
			std::filesystem::remove(db_path);

			std::ofstream db_file(db_path);
			assert(db.contains("items"));
			assert(!db["items"].empty());

			db_file << db << std::endl;
		}

		// Create a lock file to create a simple timestamp for the update
		// This should help with avoiding unnecessary db updates
		{
			std::ofstream lock_file(db_update_lock_file);
			lock_file << "Remove this file if you want to force ge-inspector to update its database\n" << random_seed();
		}
	}

	void update_item(const item& item)
	{
		nlohmann::json& db_items = DATABASE["items"];
		const auto item_it = std::find_if(db_items.begin(), db_items.end(), [item](const nlohmann::json& json_item){
			return json_item["id"] == item.id;
		});

		*item_it = item;
	}

	void write_db()
	{
		// To avoid DB corruption, first write into a temporary file and then
		// copy that file into the actual DB location

		constexpr char temp_db_path[] = "/tmp/ge-inspector-temporary-db";
		constexpr char db_backup_path[] = "/tmp/ge-inspector-db-backup";

		{
			std::ofstream tmp_db(temp_db_path);
			tmp_db << DATABASE << std::endl;
		}

		if (!std::filesystem::exists(temp_db_path))
		{
			std::cout << "couldn't write into a temporary database file at " << temp_db_path << '\n';
			exit(1);
		}

		// Create a backup of the database just in case
		std::filesystem::copy(db_path, db_backup_path);

		// Remove the current database and copy over the temporary file
		std::filesystem::remove(db_path);
		std::filesystem::copy(temp_db_path, db_path);

		// If we are still executing, delete the backup file
		std::filesystem::remove(db_backup_path);
	}

	bool update_item_member_status(item& item)
	{
		// Don't do anything if the status is already known
		if (item.members != ge::members_item::unknown)
			return false;

		std::cout << " > Updating data with item: " << item.name << std::flush;

		nlohmann::json item_details;

		try {
			item_details = download_json(item_details_json_url + std::to_string(item.id), 0);
		} catch (const nlohmann::json::exception& e) {
			std::cout << std::flush;
			std::cout << "Member item status update error with item [" << item.name << "]: " << e.what() << std::endl;
		}

		assert(item_details.contains("item"));
		assert(item_details["item"].contains("type"));

		// Use the item details to figure out the category
		const std::string item_type = item_details["item"]["type"];

		if (!item_categories.contains(item_type))
		{
			std::cout << "\nERROR!\nUnknown item type: " << item_type << "\n";
			std::cout << "Item name: " << item.name << "\n";
			std::cout << "ID: " << item.id << "\n";
			exit(1);
		}

		const u8 category_id = item_categories.at(item_type);
		std::cout << " (" << item_type << ")" << std::flush;

		// Get the member information for all items that have the same
		// first letter in the name and belong to the same category
		{
			nlohmann::json& db_items = DATABASE["items"];
			nlohmann::json category_json;
			u8 page = 1;
			constexpr u8 max_items_per_page = 12;

			do
			{
				const std::string json_url = std::format("https://secure.runescape.com/m=itemdb_rs/api/catalogue/items.json?category={}&alpha={}&page={}",
						category_id, static_cast<char>(std::tolower(item.name.at(0))), page);

				std::future<nlohmann::json> category_json_future = std::async(std::launch::async, download_json, json_url, 0);

				constexpr std::chrono::duration rate_limit_wait_duration = std::chrono::milliseconds(100);
				ge::future_spinner(category_json_future, rate_limit_wait_duration);

				category_json = category_json_future.get();
				++page;

				for (const nlohmann::json& category_item : category_json["items"])
				{
					auto item_it = std::find_if(db_items.begin(), db_items.end(), [category_item](const nlohmann::json& json_item){
						return json_item["id"] == category_item["id"];
					});

					if (item_it == db_items.end())
						continue;

					if (category_item["members"] == "true") [[likely]]
						(*item_it)["members"] = ge::members_item::yes;
					else
						(*item_it)["members"] = ge::members_item::no;

					(*item_it)["category"] = category_id;

					// Update the item we were looking for originally (if it was found)
					if ((*item_it)["id"] == item.id) [[unlikely]]
					{
						if (category_item["members"] == "true") [[likely]]
							item.members = ge::members_item::yes;
						else
							item.members = ge::members_item::no;

						item.category = category_id;
					}
				}

				// Print dots to show progress
				std::cout << '.' << std::flush;
			} while (category_json["items"].size() == max_items_per_page);
		}

		// If no data was found for the item, mark it as such
		if (item.members == ge::members_item::unknown)
		{
			item.members = ge::members_item::no_data;

			// Find the item in the database and update its information
			const auto item_it = std::find_if(DATABASE["items"].begin(), DATABASE["items"].end(), [item](const nlohmann::json& json_item){
				return json_item["id"] == item.id;
			});

			(*item_it)["members"] = ge::members_item::no_data;
		}

		// Clean up the line in case some other printing was already going on at this point
		ge::clear_current_line();

		// Update the DB incase the query gets cancelled abrubtly
		write_db();

		return true;
	}

	std::vector<item> load_db()
	{
		std::fstream db_file(db_path, std::ios::in);

		if (!db_file.is_open()) [[unlikely]]
		{
			std::cout << "No database file was found. Downloading the required data and creating a new one...\n";
			init_db();

			// Attempt to re-open the database file
			try {
				db_file.open(db_path, std::ios::in);
			} catch (const std::exception& e) {
				std::cout << "can't open the database file: " << e.what() << '\n';
			}
		}

		DATABASE = nlohmann::json::parse(db_file);

		const std::vector<item> items = DATABASE["items"];

		return items;
	}

	void update_filtered_item_data(std::vector<item>& items)
	{
		u64 db_index = 0;

		for (ge::item& item : items)
		{
			// Skip items that already have a known membership status
			if (item.members != ge::members_item::unknown) [[likely]]
				continue;

			while (db_index < DATABASE["items"].size() && item.id != DATABASE["items"][db_index]["id"])
				++db_index;

			if (db_index >= DATABASE["items"].size())
				break;

			ge::members_item members = DATABASE["items"][db_index]["members"];
			item.members = members;
		}
	}

	u64 item_cost(const std::string& name)
	{
		// Find the item in the database
		const auto it = std::find_if(DATABASE["items"].begin(), DATABASE["items"].end(), [&name](const nlohmann::json& json_item){
			return json_item["name"] == name;
		});

		return (*it)["price"];
	}

	std::vector<u64> item_price_history(item& item, const u64 days)
	{
		std::vector<u64> prices;
		std::vector<u64> trimmed_prices;
		u64 trimmed_size = days;

		// Check if the value is cached and if the cached value is new enough to be re-used
		const auto last_update = std::chrono::system_clock::time_point{ std::chrono::nanoseconds{ item.last_price_history_update } };
		const auto now = std::chrono::system_clock::now();
		const auto duration = std::chrono::duration(now - last_update);

		using namespace std::chrono_literals;

		if (duration <= 24h)
		{
			if (item.price_history.size() < days)
				trimmed_size = item.price_history.size();

			trimmed_prices.resize(trimmed_size);
			std::copy(item.price_history.end() - trimmed_size, item.price_history.end(), trimmed_prices.begin());
			return trimmed_prices;
		}

		const std::string url = std::format("https://secure.runescape.com/m=itemdb_rs/api/graph/{}.json", item.id);
		std::future<nlohmann::json> item_price_history_future = std::async(download_json, url, 0);

		do
		{
			std::cout << "\rDownloading graph data " << ge::spinner() << std::flush;
		} while (item_price_history_future.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready);

		nlohmann::json item_price_history = item_price_history_future.get();

		std::transform(item_price_history["daily"].begin(), item_price_history["daily"].end(), std::back_inserter(prices), [](const u64 value) {
			return value;
		});

		ge::clear_current_line();

		// Cache the price history
		item.price_history = prices;
		item.last_price_history_update = std::chrono::system_clock::now().time_since_epoch().count();
		update_item(item);
		write_db();

		if (prices.size() < days)
			trimmed_size = prices.size();

		trimmed_prices.resize(trimmed_size);
		std::copy(prices.end() - trimmed_size, prices.end(), trimmed_prices.begin());

		return trimmed_prices;
	}
}
