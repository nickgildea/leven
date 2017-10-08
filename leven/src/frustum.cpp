#include	"frustum.h"

// ----------------------------------------------------------------------------

using namespace glm;

// ----------------------------------------------------------------------------

Frustum BuildFrustum(const mat4& mvp)
{
	Frustum frustum;

	frustum.p[0] = vec4(mvp[0][3] - mvp[0][0], mvp[1][3] - mvp[1][0], mvp[2][3] - mvp[2][0], mvp[3][3] - mvp[3][0]);
	frustum.p[1] = vec4(mvp[0][3] + mvp[0][0], mvp[1][3] + mvp[1][0], mvp[2][3] + mvp[2][0], mvp[3][3] + mvp[3][0]);
	frustum.p[2] = vec4(mvp[0][3] + mvp[0][1], mvp[1][3] + mvp[1][1], mvp[2][3] + mvp[2][1], mvp[3][3] + mvp[3][1]);
	frustum.p[3] = vec4(mvp[0][3] - mvp[0][1], mvp[1][3] - mvp[1][1], mvp[2][3] - mvp[2][1], mvp[3][3] - mvp[3][1]);
	frustum.p[4] = vec4(mvp[0][3] - mvp[0][2], mvp[1][3] - mvp[1][2], mvp[2][3] - mvp[2][2], mvp[3][3] - mvp[3][2]);
	frustum.p[5] = vec4(mvp[0][3] + mvp[0][2], mvp[1][3] + mvp[1][2], mvp[2][3] + mvp[2][2], mvp[3][3] + mvp[3][2]);

	for (int i = 0; i < 6; i++)
	{
		frustum.p[i] = normalize(frustum.p[i]);
	}

	return frustum;
}

// ----------------------------------------------------------------------------

bool AABBInsideFrustum(const AABB& aabb, const Frustum& frustum)
{
	for (int i = 0; i < 6; i++)
	{
		const vec4& p = frustum.p[i];
		int outside = 0;
		outside += dot(p, vec4(aabb.min.x, aabb.min.y, aabb.min.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.max.x, aabb.max.y, aabb.min.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.max.x, aabb.min.y, aabb.max.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.min.x, aabb.max.y, aabb.max.z, 1.f)) < 0.f ? 1 : 0;
		outside += dot(p, vec4(aabb.max.x, aabb.max.y, aabb.max.z, 1.f)) < 0.f ? 1 : 0;

		if (outside == 8)
		{
			// all points outside the frustum
			return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------------------
