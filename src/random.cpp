#include "Random.hpp"

#include <fstream>

namespace ge
{
	u64 random_seed()
	{
		size_t seed;
		std::fstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
		urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
		return seed;
	}
}
