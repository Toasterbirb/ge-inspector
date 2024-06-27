#include "Color.hpp"

#include <algorithm>
#include <assert.h>
#include <string>

namespace ge
{
	std::string next_color(const colorscheme colorscheme)
	{
		assert(colorscheme != colorscheme::white);

		const std::string& color = colorschemes.at(colorscheme).second.at(0);

		// Rotate the colorscheme so that the color will be
		// different on next invocation
		std::rotate(
			colorschemes.at(colorscheme).second.begin(),
			colorschemes.at(colorscheme).second.begin() + 1,
			colorschemes.at(colorscheme).second.end()
		);

		return color;
	}
}
