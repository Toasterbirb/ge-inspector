#include "Random.hpp"

#include <chrono>

namespace ge
{
	u64 random_seed()
	{
		// Generate a random seed from the current time
		return std::chrono::system_clock::now().time_since_epoch().count();
	}
}
