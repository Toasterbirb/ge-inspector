#include "PrintUtils.hpp"

#include <algorithm>
#include <array>
#include <iostream>

namespace ge
{
	static std::array spinner_chars = { '-', '\\', '|', '/' };

	void clear_current_line()
	{
		std::cout << "\33[2K\r";
	}

	std::string spinner()
	{
		const std::string str = { '[', spinner_chars.at(0), ']' };
		std::rotate(spinner_chars.begin(), spinner_chars.begin() + 1, spinner_chars.end());
		return str;
	}
}
