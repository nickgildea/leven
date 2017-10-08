#include	"physics.h"

#include	"render.h" // TODO remove
#include	"camera.h"
#include	"timer.h"
#include	"volume_constants.h"
#include	"aabb.h"
#include	"slab_allocator.h"
#include	"double_buffer.h"
#include	"pool_allocator.h"
#include	"glm_hash.h"
#include	"threadpool.h"

#include	<btBulletCollisionCommon.h>
#include	<btBulletDynamicsCommon.h>
#include	<BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include	<BulletDynamics/Character/btKinematicCharacterController.h>
#include	<BulletCollision/CollisionDispatch/btGhostObject.h>

#include	<stdio.h>
#include	<vector>
#include	<functional>
#include	<deque>
#include	<mutex>
#include	<thread>
#include	<condition_variable>
#include	<atomic>
#include	"sdl_wrapper.h"
#include	<Remotery.h>
#include	<unordered_map>

using glm::vec3;
using glm::vec4;
using glm::ivec3;
using glm::mat3;

static const s16 PHYSICS_GROUP_WORLD = 1;
static const s16 PHYSICS_GROUP_ACTOR = 2;
static const s16 PHYSICS_GROUP_PLAYER = 4;
static const s16 PHYSICS_FILTER_ALL = PHYSICS_GROUP_WORLD | PHYSICS_GROUP_ACTOR | PHYSICS_GROUP_PLAYER;
static const s16 PHYSICS_FILTER_NOT_PLAYER = PHYSICS_FILTER_ALL & ~PHYSICS_GROUP_PLAYER;

// ----------------------------------------------------------------------------

struct PhysicsBody
{
	vec3				position;
	mat3                transform {1.f};
};

const int MAX_PHYSICS_BODY = 4 * 1024;
btAlignedObjectArray<btRigidBody*> g_rigidBodies;
PoolAllocator<PhysicsBody> g_bodyAlloc;
std::mutex g_bodyMutex;

// ----------------------------------------------------------------------------

struct PhysicsMeshData;

struct WorldCollisionNode
{
	ivec3				min;
	PhysicsMeshData*	mainMesh = nullptr;
	PhysicsMeshData*	seamMesh = nullptr;
	RenderMesh*			renderMainMesh = nullptr;
	RenderMesh*			renderSeamMesh = nullptr;
};

SlabAllocator<WorldCollisionNode, 512> g_worldNodeAlloc;
std::unordered_map<ivec3, WorldCollisionNode*> g_worldNodes;

AABB					g_worldBounds;

DoubleBuffer<std::vector<RenderMesh*>> g_worldRenderMeshes;

// ----------------------------------------------------------------------------

// i.e. 1 voxel is PHYSICS_SCALE meters
const float PHYSICS_SCALE = 0.05f;

template <typename T>
T Scale_WorldToPhysics(const T& worldValue)
{
	return worldValue * PHYSICS_SCALE;
}

template <typename T>
T Scale_PhysicsToWorld(const T& worldValue)
{
	return worldValue / PHYSICS_SCALE;
}

// ----------------------------------------------------------------------------

btBroadphaseInterface* g_broadphase = nullptr;
btCollisionDispatcher* g_dispatcher = nullptr;
btConstraintSolver* g_solver = nullptr;
btDefaultCollisionConfiguration* g_collisionCfg = nullptr;
btDiscreteDynamicsWorld* g_dynamicsWorld = nullptr;

struct PhysicsMeshData
{
	glm::vec4*						vertices = nullptr;
	int								numVertices = 0;

	int*							triangles = nullptr;
	int								numTriangles = 0;

	btTriangleIndexVertexArray*		buffer = nullptr;
	btBvhTriangleMeshShape*			shape = nullptr;
	btRigidBody*					body = nullptr;
};

const int MAX_PHYSICS_MESHES = 4 * 1024;
PoolAllocator<PhysicsMeshData> g_meshAlloc;
std::mutex g_meshAllocMutex;

static PhysicsMeshData* AllocMeshData()
{
	PhysicsMeshData* data = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_meshAllocMutex);
		data = g_meshAlloc.alloc();
	}
	
	*data = PhysicsMeshData();
	return data;
}

static void FreeMeshData(PhysicsMeshData* meshData)
{
	std::lock_guard<std::mutex> lock(g_meshAllocMutex);
	g_meshAlloc.free(meshData);
}

vec3 g_rayHitPosition[2];
vec3 g_rayHitNormal[2];
std::atomic<int> g_rayHitPositionCounter = 0;

// ----------------------------------------------------------------------------

typedef std::function<void(void)> PhysicsOperation;
typedef std::deque<PhysicsOperation> PhysicsOperationQueue;
PhysicsOperationQueue g_operationQueue;
std::mutex g_operationMutex;

enum PhysicsOperationType
{
	PhysicsOp_RayCast,
	PhysicsOp_WorldUpdate
};

void EnqueuePhysicsOperation(const PhysicsOperationType opType, const PhysicsOperation& op)
{
	std::lock_guard<std::mutex> lock(g_operationMutex);
	g_operationQueue.push_back(op);
}

// ----------------------------------------------------------------------------

const float PLAYER_WIDTH = 48.f;
const float PLAYER_HEIGHT = 120.f - PLAYER_WIDTH;   // the capsule height is (2 * radius + height) 
struct Player
{
	btRigidBody*              body = nullptr;
	btPairCachingGhostObject* ghost = nullptr;
	vec3                      velocity;
	bool                      falling = true;
	bool                      noclip = false;
	bool                      jump = false;
};

Player g_player;

// ----------------------------------------------------------------------------

void SpawnPlayerImpl(const glm::vec3& origin)
{
	LVN_ASSERT(g_player.body == nullptr);
	LVN_ASSERT(g_player.ghost == nullptr);
	if (g_player.body || g_player.ghost)
	{
		return;
	}

	btCapsuleShape* collisionShape = new btCapsuleShape(
		Scale_WorldToPhysics(PLAYER_WIDTH / 2), Scale_WorldToPhysics(PLAYER_HEIGHT));

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(Scale_WorldToPhysics(btVector3(origin.x, origin.y, origin.z)));

	const float mass = 10.f;
	btVector3 ineritia;

	btMotionState* motionState = new btDefaultMotionState(transform);
	collisionShape->calculateLocalInertia(mass, ineritia);

	btRigidBody::btRigidBodyConstructionInfo bodyInfo(mass, motionState, collisionShape, ineritia);
	bodyInfo.m_friction = 0.5f;
	bodyInfo.m_restitution = 0.f;
	bodyInfo.m_linearDamping = 0.f;

	g_player.body = new btRigidBody(bodyInfo);
	g_player.body->setActivationState(DISABLE_DEACTIVATION);
	g_player.body->setAngularFactor(0.f);
	g_dynamicsWorld->addRigidBody(g_player.body, PHYSICS_GROUP_PLAYER, PHYSICS_FILTER_ALL);

	g_player.ghost = new btPairCachingGhostObject;
	g_player.ghost->setCollisionShape(collisionShape);
	g_player.ghost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	g_player.ghost->setWorldTransform(transform);

	g_dynamicsWorld->addCollisionObject(g_player.ghost, PHYSICS_GROUP_PLAYER, PHYSICS_FILTER_NOT_PLAYER);
	g_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
}

// ----------------------------------------------------------------------------

void UpdatePlayer(const float deltaT, const float elapsedTime)
{
	rmt_ScopedCPUSample(UpdatePlayer);

	if (!g_player.body || !g_player.ghost)
	{
		return;
	}

	btVector3 origin = g_player.body->getWorldTransform().getOrigin();
//	origin = g_player.body->getCenterOfMassPosition();

	const float bottomOffset = (PLAYER_WIDTH / 2.f) + (PLAYER_HEIGHT / 2.f);
	const btVector3 rayEnd = origin - 
		Scale_WorldToPhysics(btVector3(0.f, bottomOffset + 0.f, 0.f));
	btCollisionWorld::ClosestRayResultCallback callback(origin, rayEnd);
	g_dynamicsWorld->rayTest(origin, rayEnd, callback);

	bool onGround = false;
	float onGroundDot = 0.f;
	
	auto pairs = g_player.ghost->getOverlappingPairCache()->getOverlappingPairArray();
	btManifoldArray manifolds;
	for (int i = 0; i < pairs.size(); i++)
	{
		manifolds.clear();
		const auto& p = pairs[i];
		btBroadphasePair* pair = g_dynamicsWorld->getPairCache()->findPair(p.m_pProxy0, p.m_pProxy1);
		if (!pair)
		{
			continue;
		}

		if (pair->m_algorithm)
		{
			pair->m_algorithm->getAllContactManifolds(manifolds);
		}

		for (int j = 0; j < manifolds.size(); j++)
		{
			auto* m = manifolds[j];
			const bool isFirstBody = m->getBody0() == g_player.ghost;
			const float dir = isFirstBody ? -1.f : 1.f;

			for (int c = 0; c < m->getNumContacts(); c++)
			{
				const auto& pt = m->getContactPoint(c);
				if (pt.getDistance() <= 1e-3)
				{
					const btVector3 p = isFirstBody ? pt.getPositionWorldOnA() : pt.getPositionWorldOnB();
					const btVector3 d = (origin - p).normalize();
					onGroundDot = glm::max(onGroundDot, d.dot(btVector3(0.f, 1.f, 0.f)));
				}
			}
		}
	}

	static float checkGroundTime = 0.f;
	if (g_player.falling && checkGroundTime < elapsedTime)
	{
		if (onGroundDot > 0.85f)
		{
			onGround = true;
			g_player.falling = false;
		}
	}

	/*
	printf("onGroundDot=%.2f falling=%s\n", 
		onGroundDot, 
		g_player.falling ? "True" : "False");
	*/

	const btVector3 currentVelocty = g_player.body->getLinearVelocity();
	vec3 inputVelocity = g_player.velocity * 750.f;
	if (!g_player.noclip)
	{
		float velocity = currentVelocty.y();
		if (!g_player.falling)
		{
			velocity = glm::min(0.f, velocity) - (10 * deltaT);
		}

		if (!g_player.falling && g_player.jump)
		{
	//		printf("jump\n");
			g_player.body->applyCentralImpulse(btVector3(0, 65, 0));
			g_player.jump = false;
			g_player.falling = true;
			checkGroundTime = elapsedTime + 0.1f;
		}
		else
		{
			g_player.body->setLinearVelocity(btVector3(inputVelocity.x, velocity, inputVelocity.z));
		}
	}
	else
	{
		g_player.body->setLinearVelocity(btVector3(inputVelocity.x, inputVelocity.y, inputVelocity.z));
	}

	g_player.ghost->getWorldTransform().setOrigin(g_player.body->getWorldTransform().getOrigin());
}

// ----------------------------------------------------------------------------

std::thread g_physicsThread;
bool g_physicsQuit = false;

void PhysicsThreadFunction()
{
	u32 prevTime = SDL_GetTicks();
	const float startTime = (float)prevTime / 1000.f;

	bool dirty = false;
	while (!g_physicsQuit)
	{
		PhysicsOperationQueue queuedOperations;
		{
			std::lock_guard<std::mutex> lock(g_operationMutex);
			queuedOperations = g_operationQueue;
			g_operationQueue.clear();
		}
		
		for (auto& op: queuedOperations)
		{
			dirty = true;
			op();
		}

		const u32 deltaTime = SDL_GetTicks() - prevTime;
		const float dt = deltaTime / 1000.f;
		const float updatePeriod = 1 / 60.f;
		if (dt < updatePeriod)
		{
			continue;
		}

		prevTime = SDL_GetTicks();
		const float elapsedTime = ((float)prevTime / 1000.f) - startTime;

		{
			rmt_ScopedCPUSample(Physics_BulletStep);
			g_dynamicsWorld->stepSimulation(dt);
		}

		if (dirty)
		{
			std::vector<RenderMesh*>* meshes = g_worldRenderMeshes.next();
			meshes->clear();
			meshes->reserve(g_worldNodes.size());

			for (const auto& pair: g_worldNodes)
			{
				const WorldCollisionNode* node = pair.second;
				if (node)
				{
					if (node->renderMainMesh)
					{
						meshes->push_back(node->renderMainMesh);
					}

					if (node->renderSeamMesh) 
					{
						meshes->push_back(node->renderSeamMesh);
					}
				}
			}

			g_worldRenderMeshes.increment();
			dirty = false;
		}
		
		{
			rmt_ScopedCPUSample(UpdateBodies);

			for (int i = 0; i < g_rigidBodies.size(); i++)
			{
				btRigidBody* body = g_rigidBodies[i];
				LVN_ASSERT(body);

				PhysicsBody* physicsBody = (PhysicsBody*)body->getUserPointer();
				LVN_ASSERT(physicsBody);

				const btTransform& transform = body->getInterpolationWorldTransform();
				const btVector3& o = transform.getOrigin();
				physicsBody->position = Scale_PhysicsToWorld(vec3(o.x(), o.y(), o.z()));

				btScalar matrix[4][4];
				transform.getOpenGLMatrix(&matrix[0][0]);

				physicsBody->transform = mat3(
					vec3(matrix[0][0], matrix[0][1], matrix[0][2]),
					vec3(matrix[1][0], matrix[1][1], matrix[1][2]),
					vec3(matrix[2][0], matrix[2][1], matrix[2][2]));
			}
		}

		UpdatePlayer(dt, elapsedTime);
	}
}

// ----------------------------------------------------------------------------

void InitialiseWorldNodes()
{
	for (int x = g_worldBounds.min.x; x < g_worldBounds.max.x; x += COLLISION_NODE_SIZE)
	for (int y = g_worldBounds.min.y; y < g_worldBounds.max.y; y += COLLISION_NODE_SIZE)
	for (int z = g_worldBounds.min.z; z < g_worldBounds.max.z; z += COLLISION_NODE_SIZE)
	{
		WorldCollisionNode* node = g_worldNodeAlloc.alloc();
		node->min = ivec3(x, y, z);
		LVN_ASSERT(g_worldNodes.find(node->min) == end(g_worldNodes));
		g_worldNodes[node->min] = node;
	}
}

// ----------------------------------------------------------------------------

void ReleaseWorldNodes()
{
	for (auto pair: g_worldNodes)
	{
		WorldCollisionNode* node = pair.second;
		FreeMeshData(node->mainMesh);
		FreeMeshData(node->seamMesh);
		if (node->renderMainMesh) Render_FreeRenderMesh(&node->renderMainMesh);
		if (node->renderSeamMesh) Render_FreeRenderMesh(&node->renderSeamMesh);
	}

	g_worldNodes.clear();
	g_worldNodeAlloc.clear();
}

// ----------------------------------------------------------------------------

bool Physics_Initialise(const AABB& worldBounds)
{
	g_worldBounds = worldBounds;

	g_meshAlloc.initialise(MAX_PHYSICS_MESHES);

	btDefaultCollisionConstructionInfo cci;
	cci.m_defaultMaxPersistentManifoldPoolSize = 1 << 15;
	g_collisionCfg = new btDefaultCollisionConfiguration(cci);
	g_dispatcher = new btCollisionDispatcher(g_collisionCfg);

	btHashedOverlappingPairCache* pairCache = new btHashedOverlappingPairCache;

	const btVector3 worldMin = btVector3(g_worldBounds.min.x, g_worldBounds.min.y, g_worldBounds.min.z);
	const btVector3 worldMax = btVector3(g_worldBounds.max.x, g_worldBounds.max.y, g_worldBounds.max.z);
	const short maxHandles = 3500;

	g_broadphase = 
		new btAxisSweep3(Scale_WorldToPhysics(worldMin), Scale_WorldToPhysics(worldMax), maxHandles, pairCache);
	g_solver = new btSequentialImpulseConstraintSolver;
	g_dynamicsWorld = new btDiscreteDynamicsWorld(g_dispatcher, g_broadphase, g_solver, g_collisionCfg);
	g_dynamicsWorld->setGravity(btVector3(0, -9.8f, 0));
	g_dynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_ENABLE_FRICTION_DIRECTION_CACHING;
	g_dynamicsWorld->getSolverInfo().m_numIterations = 5;

	g_physicsQuit = false;
	g_physicsThread = std::thread(PhysicsThreadFunction);

	InitialiseWorldNodes();

	g_bodyAlloc.initialise(MAX_PHYSICS_BODY);

	return true;
}

// ----------------------------------------------------------------------------

void Physics_Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(g_operationMutex);
		g_operationQueue.clear();
	}

	g_physicsQuit = true;
	g_physicsThread.join();

	g_dynamicsWorld->removeRigidBody(g_player.body);
	g_dynamicsWorld->removeCollisionObject(g_player.ghost);
	delete g_player.body;
	delete g_player.ghost;
	g_player = Player();

	g_worldRenderMeshes.clear();

	for (int i = 0; i < g_rigidBodies.size(); i++)
	{
		delete g_rigidBodies[i];
	}

	g_rigidBodies.clear();
	g_bodyAlloc.clear();

	ReleaseWorldNodes();

	g_meshAlloc.clear();

	delete g_dynamicsWorld; 
	g_dynamicsWorld = nullptr;

	delete g_solver;
	g_solver = nullptr;

	delete g_broadphase;
	g_broadphase = nullptr;

	delete g_dispatcher;
	g_dispatcher = nullptr;

	delete g_collisionCfg;
	g_collisionCfg = nullptr;
}

// ----------------------------------------------------------------------------

std::vector<RenderMesh*> Physics_GetRenderData(const Frustum& frustum)
{
	// Important that we return a COPY here not the actual collection
	return *g_worldRenderMeshes.current();
}

// ----------------------------------------------------------------------------

void AddMeshToWorldImpl(const glm::vec3& origin, MeshBuffer* buffer, PhysicsMeshData* meshData)
{
	rmt_ScopedCPUSample(Physics_AddMeshToWorld);

	// TODO meshData->vertices and meshData->triangles should be created on the GPU
	{
		rmt_ScopedCPUSample(Copy);

		LVN_ASSERT(!meshData->vertices);
		LVN_ASSERT(meshData->numVertices == 0);
		meshData->vertices = new vec4[buffer->numVertices];
		meshData->numVertices = buffer->numVertices;

		const vec4 offset(origin, 0.f);
		for (int i = 0; i < buffer->numVertices; i++)
		{
			meshData->vertices[i] = Scale_WorldToPhysics(buffer->vertices[i].xyz - offset);
		}

		LVN_ASSERT(!meshData->triangles);
		LVN_ASSERT(meshData->numTriangles == 0);
		meshData->triangles = new int[buffer->numTriangles * 3];
		meshData->numTriangles = buffer->numTriangles;
		memcpy(meshData->triangles, buffer->triangles, sizeof(int) * 3 * buffer->numTriangles);
	}

	{
		rmt_ScopedCPUSample(Bullet);

		btIndexedMesh indexedMesh;
		indexedMesh.m_vertexBase = (const unsigned char*)&meshData->vertices[0];
		indexedMesh.m_vertexStride = sizeof(vec4);
		indexedMesh.m_numVertices = meshData->numVertices;
		indexedMesh.m_triangleIndexBase = (const unsigned char*)&meshData->triangles[0];
		indexedMesh.m_triangleIndexStride = sizeof(int) * 3;
		indexedMesh.m_numTriangles = meshData->numTriangles;
		indexedMesh.m_indexType = PHY_INTEGER;

		meshData->buffer = new btTriangleIndexVertexArray;
		meshData->buffer->addIndexedMesh(indexedMesh);	
		meshData->shape = new btBvhTriangleMeshShape(meshData->buffer, true, true);

		const float mass = 0.f;
		meshData->body = new btRigidBody(mass, nullptr, meshData->shape);
		meshData->body->setFriction(0.9f);

		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(Scale_WorldToPhysics(btVector3(origin.x, origin.y, origin.z)));
		meshData->body->setWorldTransform(transform);
		meshData->body->setCollisionFlags(meshData->body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	}
}

// ----------------------------------------------------------------------------

void RemoveMeshData(PhysicsMeshData* meshData)
{
	rmt_ScopedCPUSample(RemoveMeshData);
	LVN_ASSERT(meshData);
	LVN_ASSERT(std::this_thread::get_id() == g_physicsThread.get_id());

	g_dynamicsWorld->removeRigidBody(meshData->body);
	delete meshData->body;
	meshData->body = nullptr;

	delete meshData->shape;
	meshData->shape = nullptr;

	delete meshData->buffer;
	meshData->buffer = nullptr;

	delete[] meshData->vertices;
	meshData->vertices = nullptr;
	meshData->numVertices = 0;

	delete[] meshData->triangles;
	meshData->triangles = nullptr;
	meshData->numTriangles = 0;
}

// ----------------------------------------------------------------------------

void ReplaceCollisionNodeMesh(
	const bool replaceMainMesh,
	WorldCollisionNode* node,
	PhysicsMeshData* newMesh)
{
	rmt_ScopedCPUSample(ReplaceCollisionMeshes);
	LVN_ASSERT(std::this_thread::get_id() == g_physicsThread.get_id());

	PhysicsMeshData* oldMesh = nullptr;
	if (replaceMainMesh)
	{
		oldMesh = node->mainMesh;
		node->mainMesh = newMesh;
	}
	else
	{
		oldMesh = node->seamMesh;
		node->seamMesh = newMesh;
	}

	if (newMesh)
	{
		g_dynamicsWorld->addRigidBody(newMesh->body, PHYSICS_GROUP_WORLD, PHYSICS_FILTER_ALL);		
	}

	if (oldMesh)
	{
		RemoveMeshData(oldMesh);
		FreeMeshData(oldMesh);
	}

	{
		rmt_ScopedCPUSample(ActivateeBodies);

		const AABB nodeBounds(node->min, COLLISION_NODE_SIZE);

		// TODO getting the lock here is terrible
		std::lock_guard<std::mutex> lock(g_bodyMutex);
		for (int i = 0; i < g_rigidBodies.size(); i++)
		{
			btRigidBody* body = g_rigidBodies[i];
			LVN_ASSERT(body);

			PhysicsBody* physicsBody = (PhysicsBody*)body->getUserPointer();
			LVN_ASSERT(physicsBody);

			if (nodeBounds.pointIsInside(ivec3(physicsBody->position)))
			{
				body->activate();
			}
		}
	}
}

// ----------------------------------------------------------------------------

void UpdateCollisionNode(const bool updateMain, WorldCollisionNode* node, MeshBuffer* meshBuffer)
{
	LVN_ASSERT(node);
	LVN_ASSERT(std::this_thread::get_id() == g_physicsThread.get_id());

	ThreadPool_ScheduleJob([=]()
	{
		PhysicsMeshData* newMesh = nullptr;
		const vec3 origin = vec3(node->min + ivec3(COLLISION_NODE_SIZE) / 2);
		
		if (meshBuffer)
		{
			newMesh = AllocMeshData();
			AddMeshToWorldImpl(origin, meshBuffer, newMesh);
		}

		if (updateMain)
		{
			node->renderMainMesh = Render_AllocRenderMesh("collision_main", meshBuffer, origin);
			if (!node->renderMainMesh)
			{
				Render_FreeMeshBuffer(meshBuffer);
			}
		}
		else
		{
			node->renderSeamMesh = Render_AllocRenderMesh("collision_seam", meshBuffer, origin);
			if (!node->renderSeamMesh)
			{
				Render_FreeMeshBuffer(meshBuffer);
			}
		}

		EnqueuePhysicsOperation(PhysicsOp_WorldUpdate,
			std::bind(ReplaceCollisionNodeMesh, updateMain, node, newMesh));
	});
}

// ----------------------------------------------------------------------------

WorldCollisionNode* FindCollisionNode(const ivec3& min)
{
	const auto iter = g_worldNodes.find(min);	
	if (iter != end(g_worldNodes))
	{
		return iter->second;
	}

	return nullptr;
}

// ----------------------------------------------------------------------------

void Physics_UpdateWorldNodeMainMesh(
	const ivec3& min,
	MeshBuffer* mainMesh)
{
	if (!g_worldBounds.pointIsInside(min))
	{
		Render_FreeMeshBuffer(mainMesh);		
		return;
	}

	WorldCollisionNode* worldNode = FindCollisionNode(min);
	if (!worldNode)
	{
		printf("Error: Unable to find collision node for min [%d %d %d]\n", min.x, min.y, min.z);
		return;
	}

	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, std::bind(UpdateCollisionNode, true, worldNode, mainMesh));
}

// ----------------------------------------------------------------------------

void Physics_UpdateWorldNodeSeamMesh(
	const ivec3& min,
	MeshBuffer* seamMesh)
{
	if (!g_worldBounds.pointIsInside(min))
	{
		Render_FreeMeshBuffer(seamMesh);		
		return;
	}

	WorldCollisionNode* worldNode = FindCollisionNode(min);
	if (!worldNode)
	{
		printf("Error: Unable to find collision node for min [%d %d %d]\n", min.x, min.y, min.z);
		return;
	}

	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, std::bind(UpdateCollisionNode, false, worldNode, seamMesh));
}

// ----------------------------------------------------------------------------

std::atomic<bool> g_rayCastPending = false;

void CastRayImpl(const glm::vec3& start, const glm::vec3& end)
{
	rmt_ScopedCPUSample(Physics_CastRay);
	LVN_ASSERT(std::this_thread::get_id() == g_physicsThread.get_id());

	const btVector3 rayStart = Scale_WorldToPhysics(btVector3(start.x, start.y, start.z));
	const btVector3 rayEnd = Scale_WorldToPhysics(btVector3(end.x, end.y, end.z));

	btCollisionWorld::AllHitsRayResultCallback callback(rayStart, rayEnd);
	callback.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
	g_dynamicsWorld->rayTest(rayStart, rayEnd, callback);

	float minFraction = FLT_MAX;

	for (int i = 0; i < callback.m_collisionObjects.size(); i++)
	{
		const btCollisionObject* object = callback.m_collisionObjects[i];
		if (callback.m_hitFractions[i] < minFraction &&
			object->getCollisionFlags() & PHYSICS_GROUP_WORLD)
		{
			minFraction = callback.m_hitFractions[i];

			const btVector3 p = callback.m_hitPointWorld[i];
			const btVector3 n = callback.m_hitNormalWorld[i];
			const int index = ++g_rayHitPositionCounter & 1;
			g_rayHitPosition[index] = Scale_PhysicsToWorld(vec3(p.x(), p.y(), p.z()));
			g_rayHitNormal[index] = glm::vec3(n.x(), n.y(), n.z());
		}
	}

	g_rayCastPending = false;
}

void Physics_CastRay(const glm::vec3& start, const glm::vec3& end)
{
	if (!g_rayCastPending)
	{
		g_rayCastPending = true;
		EnqueuePhysicsOperation(PhysicsOp_RayCast, std::bind(CastRayImpl, start, end));
	}
}

vec3 Physics_LastHitPosition()
{
	return g_rayHitPosition[g_rayHitPositionCounter & 1];
}

vec3 Physics_LastHitNormal()
{
	return g_rayHitNormal[g_rayHitPositionCounter & 1];
}

// ----------------------------------------------------------------------------

btRigidBody* SpawnShape(btCollisionShape* shape, const float mass, const vec3& origin)
{
	rmt_ScopedCPUSample(SpawnShape);

	btVector3 localInertia;
	shape->calculateLocalInertia(mass, localInertia);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(Scale_WorldToPhysics(btVector3(origin.x, origin.y, origin.z)));

	btRigidBody::btRigidBodyConstructionInfo info(mass, new btDefaultMotionState(transform), shape, localInertia);
	btRigidBody* body = new btRigidBody(info);
	body->setWorldTransform(transform);

	std::lock_guard<std::mutex> lock(g_bodyMutex);
	g_rigidBodies.push_back(body);

	return body;
}

// ----------------------------------------------------------------------------

PhysicsHandle Physics_SpawnSphere(const float radius, const float mass, const glm::vec3& origin)
{
	PhysicsBody* body = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_bodyMutex);
		body = g_bodyAlloc.alloc();
	}

	if (body)
	{
		body->position = origin;
		btSphereShape* shape = new btSphereShape(Scale_WorldToPhysics(radius));
		btRigidBody* rigidBody = SpawnShape(shape, mass, origin); 
		rigidBody->setUserPointer(body);

		EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, [=]()
		{
			g_dynamicsWorld->addRigidBody(rigidBody, PHYSICS_GROUP_WORLD, PHYSICS_FILTER_ALL);
		});

		return rigidBody;
	}

	return nullptr;
}

// ----------------------------------------------------------------------------

PhysicsHandle Physics_SpawnCube(const vec3& halfSize, const float mass, const glm::vec3& origin)
{
	PhysicsBody* body = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_bodyMutex);
		body = g_bodyAlloc.alloc();
	}

	if (body)
	{
		body->position = origin;
		btBoxShape* shape = new btBoxShape(Scale_WorldToPhysics(btVector3(halfSize.x, halfSize.y, halfSize.z)));
		btRigidBody* rigidBody = SpawnShape(shape, mass, origin); 
		rigidBody->setUserPointer(body);

		EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, [=]()
		{
			g_dynamicsWorld->addRigidBody(rigidBody, PHYSICS_GROUP_WORLD, PHYSICS_FILTER_ALL);
		});

		return rigidBody;
	}

	return nullptr;
}

// ----------------------------------------------------------------------------

glm::vec3 PhysicsBody_GetPosition(PhysicsHandle handle)
{
	if (!handle)
	{
		return vec3(0.f);
	}

	btRigidBody* body = (btRigidBody*)handle;
	PhysicsBody* physicsBody = (PhysicsBody*)body->getUserPointer();
	LVN_ASSERT(physicsBody);
	return physicsBody->position;
}

// ----------------------------------------------------------------------------

glm::mat3 PhysicsBody_GetTransform(PhysicsHandle handle)
{
	btRigidBody* body = (btRigidBody*)handle;
	PhysicsBody* physicsBody = (PhysicsBody*)body->getUserPointer();
	LVN_ASSERT(physicsBody);
	return physicsBody->transform;
}

// ----------------------------------------------------------------------------

void FreeBodyImpl(btRigidBody* body)
{
	LVN_ASSERT(body);
	LVN_ASSERT(std::this_thread::get_id() == g_physicsThread.get_id());

	g_dynamicsWorld->removeRigidBody(body);
	delete body;
}

// ----------------------------------------------------------------------------

void PhysicsBody_Free(PhysicsHandle handle)
{
	btRigidBody* body = (btRigidBody*)handle;
	PhysicsBody* physicsBody = (PhysicsBody*)body->getUserPointer();
	LVN_ASSERT(physicsBody);

	{
		std::lock_guard<std::mutex> lock(g_bodyMutex);
		g_bodyAlloc.free(physicsBody);
		g_rigidBodies.remove(body);
	}

	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, std::bind(FreeBodyImpl, body));
}

// ----------------------------------------------------------------------------

void Physics_SpawnPlayer(const vec3& origin)
{
	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, std::bind(SpawnPlayerImpl, origin));
}

// ----------------------------------------------------------------------------

vec3 Physics_GetPlayerPosition()
{
	// TODO not thread safe
	if (g_player.body)
	{
		const btVector3 origin = g_player.body->getWorldTransform().getOrigin();
		vec3 position = Scale_PhysicsToWorld(vec3(origin.x(), origin.y(), origin.z()));

		const float eyeOffset = (PLAYER_HEIGHT / 2.f) - 10.f;
		position.y += eyeOffset;
		return position;
	}

	return vec3(0.f);
}

// ----------------------------------------------------------------------------

void Physics_SetPlayerVelocity(const vec3& velocity)
{
	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, [=]()
	{
		g_player.velocity = velocity;
	});
}

// ----------------------------------------------------------------------------

void Physics_PlayerJump()
{
	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, []()
	{
		g_player.jump = true;
	});
}

// ----------------------------------------------------------------------------

void Physics_TogglePlayerNoClip()
{
	printf("noclip\n");
	EnqueuePhysicsOperation(PhysicsOp_WorldUpdate, []()
	{
		g_player.noclip = !g_player.noclip;
	});
}

// ----------------------------------------------------------------------------