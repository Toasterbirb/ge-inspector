#pragma once

#include <chrono>
#include <future>
#include <iostream>
#include <string>

namespace ge
{
	void clear_current_line();
	std::string spinner();

	/**
	 * @brief Wait for a future to finish while printing a loading spinner
	 */
	template<typename T>
	void future_spinner(const std::future<T>& f, const std::chrono::milliseconds wait_duration)
	{
		bool spinning = false;

		const size_t spinner_size = spinner().size();
		const std::string caret_ret_str(spinner_size, '\b');

		while ((spinning = (f.wait_for(wait_duration) != std::future_status::ready)))
			std::cout << spinner() << caret_ret_str << std::flush;

		// Clean out the spinner text if the spinner got printed out
		if (spinning)
		{
			const std::string empty_str(spinner_size, ' ');
			std::cout << empty_str << caret_ret_str << std::flush;
		}
	}
}
