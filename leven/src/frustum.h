#ifndef		HAS_FRUSTUM_H_BEEN_INCLUDED
#define		HAS_FRUSTUM_H_BEEN_INCLUDED

#include	<glm/glm.hpp>
#include	"aabb.h"

// ----------------------------------------------------------------------------

struct Frustum
{
	glm::vec4		p[6];
};

// ----------------------------------------------------------------------------

Frustum BuildFrustum(const glm::mat4& mvp);

// false if totally outside, true for inside and partial
bool AABBInsideFrustum(const AABB& aabb, const Frustum& frustum);

#endif	//	HAS_FRUSTUM_H_BEEN_INCLUDED