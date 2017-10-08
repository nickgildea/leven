#ifndef		HAS_AABB_BEEN_INCLUDED_H
#define		HAS_AABB_BEEN_INCLUDED_H

#include	<glm/glm.hpp>

struct AABB
{
	AABB()
		: min(0)
		, max(0)
	{
	}

	AABB(const glm::ivec3& _min, const int _size)
		: min(_min)
		, max(min + glm::ivec3(_size))
	{
	}

	AABB(const glm::ivec3& _min, const glm::ivec3& _max)
		: min(_min)
		, max(_max)
	{
	}

	bool overlaps(const AABB& other) const
	{
		return !
			(max.x < other.min.x ||
			 max.y < other.min.y ||
			 max.z < other.min.z ||
			 min.x > other.max.x ||
			 min.y > other.max.y ||
			 min.z > other.max.z);
	}

	bool pointIsInside(const glm::ivec3& point) const
	{
		return 
			(point.x >= min.x && point.x < max.x) &&
			(point.y >= min.y && point.y < max.y) &&
			(point.z >= min.z && point.z < max.z);
	}

	// from Real-time Collision Detection
	bool intersect(
		const glm::vec3& rayOrigin, 
		const glm::vec3& rayDir, 
		glm::vec3* point = nullptr, 
		float* distance = nullptr) const
	{
		float tmin = 0.f;
		float tmax = FLT_MAX;

		const glm::vec3 fmin(min);
		const glm::vec3 fmax(max);

		for (int i = 0; i < 3; i++)
		{
			if (glm::abs(rayDir[i]) < FLT_EPSILON)
			{
				if (rayOrigin[i] < fmin[i] || rayOrigin[i] >= fmax[i])
				{
					return false;
				}
			}
			else
			{
				const float ood = 1.f / rayDir[i];
				const float t1 = (fmin[i] - rayOrigin[i]) * ood;
				const float t2 = (fmax[i] - rayOrigin[i]) * ood;

				tmin = glm::max(tmin, glm::min(t1, t2));
				tmax = glm::min(tmax, glm::max(t1, t2));

				if (tmin > tmax)
				{
					return false;
				}
			}
		}

		if (distance) *distance = tmin;
		if (point) *point = rayOrigin + (rayDir * tmin);

		return true;
	}

	const glm::ivec3 getOrigin() const
	{
		const glm::ivec3 dim = max - min;
		return min + (dim / 2);
	}

	glm::ivec3		min;
	glm::ivec3		max;
};

// ----------------------------------------------------------------------------

#endif	//	HAS_AABB_BEEN_INCLUDED_H