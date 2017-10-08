#ifndef		HAS_PHYSICS_H_BEEN_INCLUDED
#define		HAS_PHYSICS_H_BEEN_INCLUDED

#include	<vector>
#include	<glm/glm.hpp>

#include	"render_types.h"
#include	"frustum.h"
#include	"aabb.h"

class RenderMesh;
//struct PhysicsBody;
typedef void* PhysicsHandle;

// ----------------------------------------------------------------------------

bool	Physics_Initialise(const AABB& worldBounds);
void	Physics_Shutdown();

std::vector<RenderMesh*> Physics_GetRenderData(const Frustum& frustum);

void Physics_UpdateWorldNodeMainMesh(
	const glm::ivec3& min,
	MeshBuffer* mainMesh);

void Physics_UpdateWorldNodeSeamMesh(
	const glm::ivec3& min,
	MeshBuffer* seamMesh);

void Physics_CastRay(const glm::vec3& start, const glm::vec3& end);
glm::vec3 Physics_LastHitPosition();
glm::vec3 Physics_LastHitNormal();

void Physics_SpawnPlayer(const glm::vec3& origin);
glm::vec3 Physics_GetPlayerPosition();
void Physics_SetPlayerVelocity(const glm::vec3& velocity);
void Physics_PlayerJump();
void Physics_TogglePlayerNoClip();

PhysicsHandle Physics_SpawnSphere(const float radius, const float mass, const glm::vec3& origin);
PhysicsHandle Physics_SpawnCube(const vec3& halfSize, const float mass, const glm::vec3& origin);
void PhysicsBody_Free(PhysicsHandle body);
glm::vec3 PhysicsBody_GetPosition(PhysicsHandle body);
glm::mat3 PhysicsBody_GetTransform(PhysicsHandle body);

// ----------------------------------------------------------------------------

#endif	//	HAS_PHYSICS_H_BEEN_INCLUDED

