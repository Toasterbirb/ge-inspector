#include "Random.hpp"

#include <array>
#include <fstream>

namespace ge
{
	u64 random_seed()
	{
		// Read in values from /dev/urandom and combine them into a singular seed

		constexpr u8 seed_value_count = 4;

		std::array<size_t, seed_value_count> seeds;
		std::fstream urandom("/dev/urandom", std::ios::in | std::ios::binary);

		// Read the values in
		for (u8 i = 0; i < seed_value_count; ++i)
			urandom.read(reinterpret_cast<char*>(&seeds[i]), sizeof(seeds.at(i)));

		size_t seed = seeds.at(0);

		// Combine everything into a singular value
		for (u8 i = 1; i < seed_value_count; ++i)
			seed = combine_hashes(seed, seeds.at(i));

		return seed;
	}
}
