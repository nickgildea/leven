#ifndef		HAS_CLIPMAP_H_BEEN_INCLUDED
#define		HAS_CLIPMAP_H_BEEN_INCLUDED

// ----------------------------------------------------------------------------

#include	"octree.h"
#include	"aabb.h"
#include	"frustum.h"
#include	"physics.h"

#include	<unordered_set>
#include	<vector>
#include	<glm/glm.hpp>
using		glm::ivec3;
using		glm::vec3;

class RenderMesh;

// ----------------------------------------------------------------------------

struct ClipmapNode
{
	ClipmapNode()
	{
		for (int i = 0; i < 8; i++)
		{
			children_[i] = nullptr;
		}
	}

	bool				active_ = false;			// TODO pack into size_?
	bool				invalidated_ = false;
	bool				empty_ = false;
	ivec3				min_;
	int					size_ = -1;
	RenderMesh*			renderMesh = nullptr;
	RenderMesh*			seamMesh = nullptr;
	OctreeNode*			seamNodes = nullptr;
	int					numSeamNodes = 0;
	ClipmapNode*		children_[8];
};

// ----------------------------------------------------------------------------

// maintain a separate octree of the currently 'active' nodes' meshes to allow
// a separation of the rendering activity and the clipmap being updated
struct ClipmapViewNode
{
	ClipmapViewNode()
	{
		for (int i = 0; i < 8; i++)
		{
			children[i] = nullptr;
		}
	}

	ivec3				min;
	int					size = -1;
	RenderMesh*			mainMesh = nullptr;
	RenderMesh*			seamMesh = nullptr;
	ClipmapViewNode*	children[8];
};

// ----------------------------------------------------------------------------

const vec3 ColourForMinLeafSize(const int minLeafSize);

// ----------------------------------------------------------------------------

class Clipmap;
struct ClipmapCollisionNode;

typedef SlabAllocator<ClipmapViewNode, 512> ClipmapViewNodeAlloc;
struct ClipmapViewTree
{
	ClipmapViewNode*		root = nullptr;
	ClipmapViewNodeAlloc*	allocator = nullptr;	
};

// ----------------------------------------------------------------------------

class Compute_MeshGenContext;

class Clipmap
{
public:

	void	initialise(
		const AABB& worldBounds);

	void	clear();

	std::vector<RenderMesh*> findVisibleNodes(const Frustum& frustum);

	ClipmapNode* findNode(const ivec3& min, const int size) const;
	std::vector<ClipmapNode*> findNodesOnPath(const ivec3& endNodeMin, const int endNodeSize);

	void update(
		const glm::vec3& cameraPosition, 
		const Frustum& frustum);

	void updateRenderState();

	ClipmapNode* findNodeContainingChunk(const ivec3& min) const;
	std::vector<ClipmapNode*> findActiveNodes(const ClipmapNode* node) const;
	ClipmapNode* findNodeForChunk(const glm::ivec3& min) const;

	void findIntersectingCollisionVolumes(
		const vec3& rayOrigin, 
		const vec3& rayDir,
		std::vector<ivec3>& volumes,
		std::vector<vec3>& intersectPositions);

	std::vector<ClipmapNode*> findNodesInsideAABB(
		const AABB& aabb) const;

	void queueCSGOperation(
		const vec3& origin, 
		const glm::vec3& brushSize, 
		const RenderShape brushShape,
		const int brushMaterial, 
		const bool isAddOperation);

	void processCSGOperations();

private:

	void    constructTree();
	void    processCSGOperationsImpl();

	void	loadCollisionNodes(
		const std::vector<glm::ivec3>& requestedNodes,
		const std::vector<glm::ivec3>& requestedSeamNodes);

	ClipmapNode*        root_ = nullptr;
	ClipmapViewTree     viewTree_;
	AABB                worldBounds_;

	Compute_MeshGenContext* clipmapMeshGen_ = nullptr;
	Compute_MeshGenContext* physicsMeshGen_ = nullptr;
};

// ----------------------------------------------------------------------------

#endif	// HAS_CLIPMAP_H_BEEN_INCLUDED