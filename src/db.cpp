#include "DB.hpp"
#include "ItemSortLambdas.hpp"

#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <vector>

namespace ge
{
	static const std::string db_dir_path = std::string(getenv("HOME")) + "/.local/share/ge-inspector";
	static const std::string db_path = db_dir_path + "/price.json";

	void to_json(nlohmann::json& j, const item& i)
	{
		j = nlohmann::json {
			{ "name", i.name },
			{ "price", i.price },
			{ "prev_price", i.prev_price },
			{ "limit", i.limit },
			{ "volume", i.volume },
			{ "high_alch", i.high_alch }
		};
	}

	void from_json(const nlohmann::json& j, item& i)
	{
		j.at("name").get_to(i.name);
		j.at("price").get_to(i.price);
		j.at("prev_price").get_to(i.prev_price);
		j.at("limit").get_to(i.limit);
		j.at("volume").get_to(i.volume);
		j.at("high_alch").get_to(i.high_alch);
	}

	static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
	{
		static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
		return size * nmemb;
	}

	nlohmann::json download_json(const std::string& url)
	{
		CURL* curl;
		CURLcode res;
		std::string read_buffer;

		struct curl_slist* headers = NULL;
		curl_slist_append(headers, "accept: application/json");

		curl = curl_easy_init();

		if (curl)
		{
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "grand-exchange-inspector");
			res = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			return nlohmann::json::parse(read_buffer);
		}
		else
		{
			std::cout << "error during curl initialization\n";
			return nlohmann::json();
		}
	}

	void update_db()
	{
		std::future<nlohmann::json> price_data_future = std::async(std::launch::async,
				download_json,
				"https://runescape.wiki/?title=Module:GEPrices/data.json&action=raw&ctype=application%2Fjson");

		std::future<nlohmann::json> limit_data_future = std::async(std::launch::async,
				download_json,
				"https://runescape.wiki/?title=Module:GELimits/data.json&action=raw&ctype=application%2Fjson");

		std::future<nlohmann::json> prev_price_data_future = std::async(std::launch::async,
				download_json,
				"https://runescape.wiki/?title=Module:LastPrices/data.json&action=raw&ctype=application%2Fjson");

		std::future<nlohmann::json> volume_data_future = std::async(std::launch::async,
				download_json,
				"https://runescape.wiki/?title=Module:GEVolumes/data.json&action=raw&ctype=application%2Fjson");

		std::future<nlohmann::json> high_alch_data_future = std::async(std::launch::async,
				download_json,
				"https://runescape.wiki/?title=Module:GEHighAlchs/data.json&action=raw&ctype=application%2Fjson");

		nlohmann::json db;

		// Process the data
		{
			nlohmann::json price_data = price_data_future.get();
			nlohmann::json limit_data = limit_data_future.get();
			nlohmann::json prev_price_data = prev_price_data_future.get();
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
					i.value(),
					limit_data[i.key()]
				};

				if (prev_price_data.contains(i.key()))
					item.prev_price = prev_price_data.at(i.key());
				else
					item.prev_price = i.value();

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

	std::vector<item> load_db()
	{
		std::fstream db_file(db_path, std::ios::in);
		nlohmann::json db = nlohmann::json::parse(db_file);

		std::vector<item> items = db["items"];

		return items;
	}

	void sort_items(std::vector<item>& items, const sort_mode mode)
	{
		switch (mode)
		{
			case volume:
				std::sort(items.begin(), items.end(), ge::sort_by_volume);
				break;

			case price:
				std::sort(items.begin(), items.end(), ge::sort_by_price);
				break;

			case alch:
				std::sort(items.begin(), items.end(), ge::sort_by_alch);
				break;

			case cost:
				std::sort(items.begin(), items.end(), ge::sort_by_total_cost);
				break;

			case limit:
				std::sort(items.begin(), items.end(), ge::sort_by_limit);
				break;

			case none:
				break;
		}
	}
}
