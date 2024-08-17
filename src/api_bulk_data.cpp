#include "APIBulkData.hpp"
#include "CURL.hpp"

#include <future>

constexpr char price_json_url[] = "https://runescape.wiki/?title=Module:GEPrices/data.json&action=raw&ctype=application%2Fjson";
constexpr char id_json_url[] = "https://runescape.wiki/?title=Module:GEIDs/data.json&action=raw&ctype=application%2Fjson";
constexpr char limit_json_url[] = "https://runescape.wiki/?title=Module:GELimits/data.json&action=raw&ctype=application%2Fjson";
constexpr char volume_json_url[] = "https://runescape.wiki/?title=Module:GEVolumes/data.json&action=raw&ctype=application%2Fjson";
constexpr char high_alch_json_url[] = "https://runescape.wiki/?title=Module:GEHighAlchs/data.json&action=raw&ctype=application%2Fjson";

namespace ge
{
	void remove_update_time_stamps(nlohmann::json& json)
	{
		json.erase("%LAST_UPDATE%");
		json.erase("%LAST_UPDATE_F%");
	}

	api_bulk_data::api_bulk_data()
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

		price = price_data_future.get();
		remove_update_time_stamps(price);

		id = id_data_future.get();
		remove_update_time_stamps(id);

		limit = limit_data_future.get();
		remove_update_time_stamps(limit);

		volume = volume_data_future.get();
		remove_update_time_stamps(volume);

		high_alch = high_alch_data_future.get();
		remove_update_time_stamps(high_alch);
	}
}
