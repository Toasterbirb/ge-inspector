#include "PrintUtils.hpp"

#include <iostream>

namespace ge
{
	void clear_current_line()
	{
		std::cout << "\33[2K\r";
	}
}
