#include	"actors.h"

#include	"render.h"
#include	"physics.h"
#include	"pool_allocator.h"

#include	<unordered_set>
#include	<mutex>

using		glm::ivec3;

// ----------------------------------------------------------------------------

struct Actor
{
	RenderMesh*      mesh = nullptr;
	PhysicsHandle    body = nullptr;
};

const int MAX_ACTORS = 4 * 1024;		// arbitrary
static PoolAllocator<Actor> g_actorAlloc;
static std::unordered_set<Actor*> g_actors;
static std::mutex g_actorMutex;
static AABB g_worldBounds;

// ----------------------------------------------------------------------------

void Actor_Initialise(const AABB& worldBounds)
{
	g_actorAlloc.initialise(MAX_ACTORS);
	g_worldBounds = worldBounds;
}

// ----------------------------------------------------------------------------

void Actor_Shutdown()
{
	g_actorAlloc.clear();
	g_actors.clear();
}

// ----------------------------------------------------------------------------

Actor* Actor_Spawn(const ActorShape shape, const vec3& colour, const float size, const vec3& position)
{
	Actor* actor = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_actorMutex);
		actor = g_actorAlloc.alloc();
	}

	if (actor)
	{
		RenderShape renderShape = RenderShape_None;
		if (shape == ActorShape_Cube)
		{
			renderShape = RenderShape_Cube;
			actor->body = Physics_SpawnCube(vec3(size / 2.f), 10.f, position);
		}
		else
		{
			renderShape = RenderShape_Sphere;
			actor->body = Physics_SpawnSphere(size, 10.f, position);
		}

		LVN_ASSERT(renderShape != RenderShape_None);
		LVN_ASSERT(actor->body);

		if (ActorMeshBuffer* buffer = Render_CreateActorMesh(renderShape, size))
		{
			actor->mesh = Render_AllocRenderMesh("actor", buffer);
			actor->mesh->setPosition(position);
			actor->mesh->setColour(vec4(colour, 0.f));
		}
	
		LVN_ASSERT(actor->mesh);
		LVN_ASSERT(actor->body);

		std::lock_guard<std::mutex> lock(g_actorMutex);
		g_actors.insert(actor);
	}

	return actor;
}

// ----------------------------------------------------------------------------

void Actor_Destroy(Actor* actor)
{
	LVN_ASSERT(actor);
	LVN_ASSERT(actor->mesh);
	LVN_ASSERT(actor->body);
	
	Render_FreeRenderMesh(&actor->mesh);
	PhysicsBody_Free(actor->body);

	std::lock_guard<std::mutex> lock(g_actorMutex);
	g_actors.erase(actor);
	g_actorAlloc.free(actor);
}

// ----------------------------------------------------------------------------

void Actor_Update()
{
	std::vector<Actor*> outOfBoundsActors;
	for (Actor* actor: g_actors)
	{
		const vec3 position = PhysicsBody_GetPosition(actor->body);
		if (!g_worldBounds.pointIsInside(ivec3(position)))
		{
			outOfBoundsActors.push_back(actor);
			continue;
		}

		RenderMesh* mesh = actor->mesh;
		mesh->setPosition(position);
		mesh->setTransform(PhysicsBody_GetTransform(actor->body));
	}

	for (Actor* actor: outOfBoundsActors)
	{
		Actor_Destroy(actor);
	}
}

// ----------------------------------------------------------------------------

std::vector<RenderMesh*> Actor_GetVisibleActors(const Frustum& frustum)
{
	// TODO frustum cull
	std::vector<RenderMesh*> meshes;

	for (Actor* actor: g_actors)
	{
		RenderMesh* mesh = actor->mesh;
		meshes.push_back(mesh);
	}

	return meshes;
}

// ----------------------------------------------------------------------------
