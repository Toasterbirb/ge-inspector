#include "CURL.hpp"

#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>

namespace ge
{
	static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
	{
		static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
		return size * nmemb;
	}

	nlohmann::json download_json(const std::string& url, u8 depth)
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
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "grand-exchange-inspector");
			res = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			// Replace all non-ascii chars with underscores
			for (size_t i = 0; i < read_buffer.size(); ++i)
			{
				if (read_buffer[i] < 0) [[unlikely]]
					read_buffer[i] = '_';
			}

			// Retry up-to 3 times if the cURL buffer is empty for some reason
			if (read_buffer.empty() && depth < 3)
			{
				return download_json(url, depth++);
			}
			else if (read_buffer.empty())
			{
				std::cout << "Could not download data from " << url << ". Try again later...\n";
				exit(1);
			}
			else
			{
				try
				{
					return nlohmann::json::parse(read_buffer);
				}
				catch (const nlohmann::json::exception& e)
				{
					std::cout << "\nException happened when trying to access URL: " << url << "\n";
					std::cout << "Error: " << e.what() << '\n';
					std::cout << "Server response: " << read_buffer << '\n';
					exit(1);
				}
			}
		}
		else
		{
			std::cout << "error during curl initialization\n";
			return nlohmann::json();
		}
	}
}
