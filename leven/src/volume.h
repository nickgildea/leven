#ifndef		HAS_VOLUME_H_BEEN_INCLUDED
#define		HAS_VOLUME_H_BEEN_INCLUDED

#include	<stdint.h>
#include	<unordered_map>
#include	<vector>
#include	<memory>
#include	<mutex>
#include	<functional>

#include	<glm/glm.hpp>
using glm::vec3;
using glm::ivec3;
using glm::ivec2;

#include	"aabb.h"
#include	"volume_materials.h"
#include	"volume_constants.h"
#include	"lrucache.h"
#include	"pool_allocator.h"
#include	"clipmap.h"
#include	"frustum.h"

#undef near
#undef far

// ----------------------------------------------------------------------------

const ivec3 ChunkMinForPosition(const ivec3& p);
const ivec3 ChunkMinForPosition(const int x, const int y, const int z);

// ----------------------------------------------------------------------------

class Volume
{
public:

	void initialise(const glm::ivec3& cameraPosition, const AABB& worldBounds);
	void destroy();

	void updateChunkLOD(const glm::vec3& currentPos, const Frustum& frustum);

	// ----------------------------------------------------------------------------
		
	std::vector<RenderMesh*> findVisibleMeshes(const Frustum& frustum) 
	{
		return clipmap_.findVisibleNodes(frustum);
	}

	// ----------------------------------------------------------------------------

	void applyCSGOperation(
		const vec3& origin, 
		const glm::vec3& brushSize, 
		const RenderShape renderShape,
		const int brushMaterial, 
		const bool rotateToCamera,
		const bool isAddOperation);

	void processCSGOperations();

private:

	void Task_UpdateChunkLOD(const glm::vec3 currentPos, const Frustum frustum);

	void processCSGOperationsImpl();

	Clipmap					clipmap_;
};

#endif	//	HAS_VOLUME_H_BEEN_INCLUDED
