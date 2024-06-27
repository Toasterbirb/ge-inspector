#include "CURL.hpp"
#include "DB.hpp"
#include "Item.hpp"

#include <assert.h>
#include <filesystem>
#include <format>
#include <fstream>
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

	constexpr char price_json_url[] = "https://runescape.wiki/?title=Module:GEPrices/data.json&action=raw&ctype=application%2Fjson";
	constexpr char id_json_url[] = "https://runescape.wiki/?title=Module:GEIDs/data.json&action=raw&ctype=application%2Fjson";
	constexpr char limit_json_url[] = "https://runescape.wiki/?title=Module:GELimits/data.json&action=raw&ctype=application%2Fjson";
	constexpr char volume_json_url[] = "https://runescape.wiki/?title=Module:GEVolumes/data.json&action=raw&ctype=application%2Fjson";
	constexpr char high_alch_json_url[] = "https://runescape.wiki/?title=Module:GEHighAlchs/data.json&action=raw&ctype=application%2Fjson";

	constexpr char item_details_json_url[] = "https://services.runescape.com/m=itemdb_rs/api/catalogue/detail.json?item=";

	void init_db()
	{
		std::future<nlohmann::json> price_data_future = std::async(std::launch::async,
				download_json, price_json_url, 0);

		std::future<nlohmann::json> id_data_future = std::async(std::launch::async,
				download_json, id_json_url, 0);

		std::future<nlohmann::json> limit_data_future = std::async(std::launch::async,
				download_json, limit_json_url, 0);

		std::future<nlohmann::json> volume_data_future = std::async(std::launch::async,
				download_json, volume_json_url, 0);

		std::future<nlohmann::json> high_alch_data_future = std::async(std::launch::async,
				download_json, high_alch_json_url, 0);

		nlohmann::json db;

		// Process the data
		{
			nlohmann::json price_data = price_data_future.get();
			nlohmann::json id_data = id_data_future.get();
			nlohmann::json limit_data = limit_data_future.get();
			nlohmann::json volume_data = volume_data_future.get();
			nlohmann::json high_alch_data = high_alch_data_future.get();

			// Skip the first two values since they are useless
			auto json_begin = price_data.begin();
			json_begin++;
			json_begin++;

			for (auto i = json_begin; i != price_data.end(); ++i)
			{
				item item {
					i.key(),
					id_data[i.key()],
					i.value(),
					limit_data[i.key()]
				};

				if (volume_data.contains(i.key()))
					item.volume = volume_data.at(i.key());

				if (high_alch_data.contains(i.key()))
					item.high_alch = high_alch_data.at(i.key());

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

		// Download the latest price and volume data
		std::future<nlohmann::json> price_data_future = std::async(std::launch::async,
				download_json, price_json_url, 0);

		std::future<nlohmann::json> volume_data_future = std::async(std::launch::async,
				download_json, volume_json_url, 0);

		// There might be new items that are not yet in the database
		std::vector<std::string> new_items;

		nlohmann::json db;

		// Update the database data
		{
			std::ifstream db_file(db_path);
			db = nlohmann::json::parse(db_file);

			nlohmann::json price_data = price_data_future.get();
			price_data.erase("%LAST_UPDATE%");
			price_data.erase("%LAST_UPDATE_F%");

			nlohmann::json volume_data = volume_data_future.get();
			volume_data.erase("%LAST_UPDATE%");
			volume_data.erase("%LAST_UPDATE_F%");

			assert(!db["items"].empty());
			for (size_t i = 0; i < db["items"].size(); ++i)
			{
				const std::string& item_name = db["items"][i]["name"];
				assert(price_data.contains(item_name));

				db["items"][i]["price"] = price_data[item_name];

				if (volume_data.contains(item_name))
					db["items"][i]["volume"] = volume_data.at(item_name);
				else
					db["items"][i]["volume"] = 0;

				price_data.erase(item_name);
				volume_data.erase(item_name);
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
	}

	void update_item(const item& item)
	{
		nlohmann::json& db_items = DATABASE["items"];
		auto item_it = std::find_if(db_items.begin(), db_items.end(), [item](const nlohmann::json& json_item){
			return json_item["id"] == item.id;
		});

		*item_it = item;
	}

	void write_db()
	{
		std::ofstream file(db_path);
		file << DATABASE << std::endl;
	}

	bool update_item_member_status(item& item)
	{
		// Don't do anything if the status is already known
		if (item.members != ge::members_item::unknown)
			return false;

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

		// Get the member information for all items that have the same
		// first letter in the name and belong to the same category
		{
			nlohmann::json& db_items = DATABASE["items"];
			nlohmann::json category_json;
			u8 page = 1;
			constexpr u8 pages_to_download_at_once = 2;

			do
			{
				std::vector<std::future<nlohmann::json>> category_item_pages;

				for (u8 i = 0; i < pages_to_download_at_once; ++i)
				{
					const std::string json_url = std::format("https://secure.runescape.com/m=itemdb_rs/api/catalogue/items.json?category={}&alpha={}&page={}",
							category_id, static_cast<char>(std::tolower(item.name.at(0))), page);

					category_item_pages.emplace_back(std::async(std::launch::async, download_json, json_url, 0));
					++page;
				}


				for (u8 i = 0; i < category_item_pages.size(); ++i)
				{
					category_json = category_item_pages.at(i).get();

					for (const nlohmann::json& category_item : category_json["items"])
					{
						auto item_it = std::find_if(db_items.begin(), db_items.end(), [category_item](const nlohmann::json& json_item){
							return json_item["id"] == category_item["id"];
						});

						if (item_it == db_items.end())
							continue;

						if (category_item["members"] == "true")
							(*item_it)["members"] = ge::members_item::yes;
						else
							(*item_it)["members"] = ge::members_item::no;

						// Update the item we were looking for originally (if it was found)
						if ((*item_it)["id"] == item.id)
						{
							if (category_item["members"] == "true")
								item.members = ge::members_item::yes;
							else
								item.members = ge::members_item::no;
						}
					}
				}
			} while (!category_json["items"].empty());
		}

		// If no data was found for the item, mark it as such
		if (item.members == ge::members_item::unknown)
		{
			item.members = ge::members_item::no_data;

			// Find the item in the database and update its information
			auto item_it = std::find_if(DATABASE["items"].begin(), DATABASE["items"].end(), [item](const nlohmann::json& json_item){
				return json_item["id"] == item.id;
			});

			(*item_it)["members"] = ge::members_item::no_data;
		}

		// Update the DB incase the query gets cancelled abrubtly
		write_db();

		return true;
	}

	std::vector<item> load_db()
	{
		std::fstream db_file(db_path, std::ios::in);

		if (!db_file.is_open())
		{
			std::cout << "No database file was found. Downloading the required data and creating a new one...\n";
			init_db();

			// Attempt to re-open the database file
			try {
				db_file.open(db_path, std::ios::in);
			} catch (std::exception e) {
				std::cout << "can't open the database file: " << e.what() << '\n';
			}
		}

		DATABASE = nlohmann::json::parse(db_file);

		std::vector<item> items = DATABASE["items"];

		return items;
	}

	void update_filtered_item_data(std::vector<item>& items)
	{
		u64 db_index = 0;

		for (ge::item& item : items)
		{
			// Skip items that already have a known membership status
			if (item.members != ge::members_item::unknown)
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
		auto it = std::find_if(DATABASE["items"].begin(), DATABASE["items"].end(), [&name](const nlohmann::json& json_item){
			return json_item["name"] == name;
		});

		return (*it)["price"];
	}
}
