#include    "random.h"

#include    <random>

std::mt19937 g_prng;

s32 RandomS32(const s32 min, const s32 max)
{
	std::uniform_int_distribution<s32> dist(min, max);
	return dist(g_prng);
}

float RandomF32(const float min, const float max)
{
	std::uniform_real_distribution<float> dist(min, max);
	return dist(g_prng);
}

glm::vec3 RandomColour()
{
	std::uniform_real_distribution<float> dist(0.f, 1.f);
	return glm::vec3(dist(g_prng), dist(g_prng), dist(g_prng));
}
