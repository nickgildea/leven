#ifndef		HAS_ACTORS_H_BEEN_INCLUDED
#define		HAS_ACTORS_H_BEEN_INCLUDED

#include	"frustum.h"
#include	"aabb.h"

#include	<vector>
#include	<glm/glm.hpp>
using       glm::vec3;
using       glm::mat3;

class RenderMesh;
struct Actor;

enum ActorShape
{
	ActorShape_Cube,
	ActorShape_Sphere,
};

void Actor_Initialise(const AABB& worldBounds);
void Actor_Shutdown();
void Actor_Update();

Actor* Actor_Spawn(const ActorShape shape, const vec3& colour, const float size, const vec3& position);
void Actor_Destroy(Actor* actor);

void Actor_SetPosition(const vec3& position);
void Actor_SetTransform(const mat3& transform);

std::vector<RenderMesh*> Actor_GetVisibleActors(const Frustum& frustum);

#endif	//	HAS_ACTORS_H_BEEN_INCLUDED
