#pragma once

#include <map>
#include <string>
#include <vector>

namespace ge
{
	enum class colorscheme
	{
		white,
		red,
		green,
		yellow,
		blue,
		purple,
		cyan,
		gray,
		checker,
		rainbow,
		rainbow_dark
	};

	static inline std::map<colorscheme, std::pair<std::string, std::vector<std::string>>> colorschemes = {
		{ colorscheme::red,				{ "red",			{ "1;31", "31" } } },
		{ colorscheme::green,			{ "green",			{ "1;32", "32" } } },
		{ colorscheme::yellow,			{ "yellow",			{ "1;33", "33" } } },
		{ colorscheme::blue,			{ "blue",			{ "1;34", "34" } } },
		{ colorscheme::purple,			{ "purple",			{ "1;35", "35" } } },
		{ colorscheme::cyan,			{ "cyan",			{ "1;36", "36" } } },
		{ colorscheme::gray,			{ "gray",			{ "37", "1;30" } } },
		{ colorscheme::checker,			{ "checker",		{ "1;37", "37" } } },
		{ colorscheme::rainbow,			{ "rainbow",		{ "1;31", "1;32", "1;33", "1;34", "1;35", "1;36" } } },
		{ colorscheme::rainbow_dark,	{ "rainbow_dark",	{ "31", "32", "33", "34", "35", "36" } } },
	};

	// Returns ANSI color escape codes
	std::string next_color(const colorscheme colorscheme);
}
