#ifndef		HAS_GLM_HASH_H_BEEN_INCLUDED
#define		HAS_GLM_HASH_H_BEEN_INCLUDED

#include	<glm/glm.hpp>

namespace std
{
	template <>
	struct hash<glm::ivec4>
	{
		std::size_t operator()(const glm::ivec4& min) const
		{
			auto x = std::hash<int>()(min.x);
			auto y = std::hash<int>()(min.y);
			auto z = std::hash<int>()(min.z);
			auto w = std::hash<int>()(min.w);

			return x ^ (((y >> 3) ^ (z << 5)) ^ (w << 8));
		}
	};

	template <>
	struct hash<glm::ivec3>
	{
		std::size_t operator()(const glm::ivec3& min) const
		{
			auto x = std::hash<int>()(min.x);
			auto y = std::hash<int>()(min.y);
			auto z = std::hash<int>()(min.z);

			return x ^ ((y >> 3) ^ (z << 5));
		}
	};
	
	template <>
	struct hash<glm::ivec2>
	{
		std::size_t operator()(const glm::ivec2& min) const
		{
			auto x = std::hash<int>()(min.x);
			auto y = std::hash<int>()(min.y);

			return x ^ (y >> 3);
		}
	};
}

#endif	//	HAS_GLM_HASH_H_BEEN_INCLUDED