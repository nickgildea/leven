#include	"clipmap.h"

#include	"volume_constants.h"
#include	"contour_constants.h"
#include	"render.h"
#include	"compute.h"
#include	"pool_allocator.h"
#include	"glm_hash.h"
#include	"random.h"
#include	"ng_mesh_simplify.h"
#include	"options.h"

#include	<glm/ext.hpp>
#include	<unordered_set>
#include	<unordered_map>
#include	<algorithm>
#include	<atomic>
#include	<deque>
#include	<mutex>
#include	<Remotery.h>

using glm::ivec3;
using glm::ivec4;
using glm::vec4;
using glm::vec3;

// TODO bad
const int NUM_LODS = 6;

// i.e. the size of the largest node that will render
const int LOD_MAX_NODE_SIZE = CLIPMAP_LEAF_SIZE * (1 << (NUM_LODS - 1));	

const float LOD_ACTIVE_DISTANCES[NUM_LODS] = 
{ 
	0.f,
	CLIPMAP_LEAF_SIZE * 1.5f,
	CLIPMAP_LEAF_SIZE * 3.5f,
	CLIPMAP_LEAF_SIZE * 5.5f,
	CLIPMAP_LEAF_SIZE * 7.5f,
	CLIPMAP_LEAF_SIZE * 13.5f,
};

int g_debugDrawBuffer = -1;

// ----------------------------------------------------------------------------

typedef std::function<void(void)> DeferredClipmapOperation;
typedef std::deque<DeferredClipmapOperation> DeferredClipmapOperationQueue;
DeferredClipmapOperationQueue g_deferredClipmapOperations;
std::mutex g_deferredClipmapOpsMutex;

void EnqueueClipmapOperation(DeferredClipmapOperation opFunc)
{
	std::lock_guard<std::mutex> lock(g_deferredClipmapOpsMutex);
	g_deferredClipmapOperations.push_back(opFunc);
}

DeferredClipmapOperationQueue GetAllQueuedClipmapOperations()
{
	std::lock_guard<std::mutex> lock(g_deferredClipmapOpsMutex);
	DeferredClipmapOperationQueue q = g_deferredClipmapOperations;
	g_deferredClipmapOperations.clear();

	return q;
}

// ----------------------------------------------------------------------------

// use double-buffered allocators to hold the nodes for the view tree
ClipmapViewNodeAlloc	g_viewAllocators[2];
std::atomic<int>		g_viewAllocCurrent = 0;

// need to free the meshes only after the render thread has new data to work with
struct ClipmapViewUpdateInfo
{
	ClipmapViewTree		updatedTree;
	std::vector<RenderMesh*>	invalidatedMeshes;
};

// simple thread communication via the 'pending' bool 
std::atomic<bool>		g_updatePending;
ClipmapViewUpdateInfo	g_updateInfo;

// ----------------------------------------------------------------------------

struct ClipmapCollisionNode
{
	ClipmapCollisionNode()
	{
	}

	ClipmapCollisionNode(const ivec3& _min)
		: min(_min)
	{
	}

	ivec3		min;
	int			numSeamNodes = 0;
	OctreeNode* seamNodes = nullptr;
};

void ReleaseCollisionNode(
	ClipmapCollisionNode* node)
{
	if (!node)
	{
		return;
	}

	delete[] node->seamNodes;
	*node = ClipmapCollisionNode();
}

const int MAX_COLLISION_NODES = 16 * 1024;
PoolAllocator<ClipmapCollisionNode> g_clipmapCollisionNodeAllocator;
std::unordered_map<ivec3, ClipmapCollisionNode*> g_clipmapCollisionNodes;

// ----------------------------------------------------------------------------

u64 EncodeNodeMin(const ivec3& min)
{
	// only called for visible nodes so no need to query voxelsPerChunk
	const ivec3 scaledMin = min / CLIPMAP_LEAF_SIZE;
	return ((u64)scaledMin.x << 40) | ((u64)scaledMin.y << 20) | scaledMin.z;
}

// ----------------------------------------------------------------------------

int ChildIndex(const ivec3& parentMin, const ivec3& childMin)
{
	// this works since child min will always be >= parent min for any axis
//	LVN_ASSERT(childMin.x >= parentMin.x);
//	LVN_ASSERT(childMin.y >= parentMin.y);
//	LVN_ASSERT(childMin.z >= parentMin.z);

	const ivec3 delta(
		(childMin.x - parentMin.x) > 0 ? 1 : 0,
		(childMin.y - parentMin.y) > 0 ? 1 : 0,
		(childMin.z - parentMin.z) > 0 ? 1 : 0);
	const int index = (delta.x << 2) | (delta.y << 1) | delta.z;
	LVN_ASSERT(index >= 0 && index < 8);
	return index;
}

// ----------------------------------------------------------------------------

void PropagateEmptyStateDownward(ClipmapNode* node)
{
	if (!node)
	{
		return;
	}

	node->empty_ = true;

	for (int i = 0; i < 8; i++)
	{
		PropagateEmptyStateDownward(node->children_[i]);
	}
}

// ----------------------------------------------------------------------------

// after the 'check for empty' call node of one size/depth will be potentially marked emtpy
// propage this state upward by checking if all the children of a node are empty
bool PropagateEmptyStateUpward(ClipmapNode* node, const int emptyNodeSize)
{
	if (!node)
	{
		return true;
	}

	if (node->size_ == emptyNodeSize)
	{
		return node->empty_;
	}
	else 
	{
		node->empty_ = true;
		for (int i = 0; i < 8; i++)
		{
			node->empty_ = node->empty_ && PropagateEmptyStateUpward(node->children_[i], emptyNodeSize);
		}

		return node->empty_;
	}
}

// ----------------------------------------------------------------------------

// for debuging, find the first empty node on each branch and add to the list
void FindEmptyNodes(ClipmapNode* node, std::vector<ClipmapNode*>& emptyNodes)
{
	if (!node)
	{
		return;
	}

	if (node->empty_)
	{
		emptyNodes.push_back(node);
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			FindEmptyNodes(node->children_[i], emptyNodes);
		}
	}

	return;
}

// ----------------------------------------------------------------------------

std::vector<ClipmapViewNode*> ConstructClipmapViewNodeParents(
	ClipmapViewNodeAlloc* allocator,
	const std::vector<ClipmapViewNode*>& childNodes,
	const ivec3& rootMin)
{
	std::unordered_map<u64, ClipmapViewNode*> parentsHashmap;
	std::vector<ClipmapViewNode*> parents;

	const int parentSize = childNodes[0]->size * 2;
	for (ClipmapViewNode* node: childNodes)
	{
		const ivec3 localPos = node->min - rootMin;
		const ivec3 parentPos = node->min - (localPos % parentSize);

		ClipmapViewNode* parentNode = nullptr;

		// need to use the local parent position to prevent -ve values 
		const u64 code = EncodeNodeMin(parentPos - rootMin);
		const auto iter = parentsHashmap.find(code);
		if (iter == end(parentsHashmap))
		{
			parentNode = allocator->alloc();
			parentNode->min = parentPos;
			parentNode->size = parentSize;

			parentsHashmap[code] = parentNode;
			parents.push_back(parentNode);
		}
		else
		{
			parentNode = iter->second;
		}

		LVN_ASSERT(parentNode->min == parentPos);

		const int childIndex = ChildIndex(parentNode->min, node->min);
		LVN_ASSERT(!parentNode->children[childIndex]);
		parentNode->children[childIndex] = node;
	}

	return parents;
}

// ----------------------------------------------------------------------------

ClipmapViewTree ConstructClipmapViewTree(
	const std::vector<ClipmapNode*>& activeNodes,
	const ivec3& rootMin)
{
	rmt_ScopedCPUSample(ConstructViewTree);

	ClipmapViewTree tree;
	tree.allocator = &g_viewAllocators[g_viewAllocCurrent++ & 1];
	tree.allocator->clear();

	if (activeNodes.empty())
	{
		return tree;
	}

	std::vector<ClipmapViewNode*> viewNodes(activeNodes.size());
	for (int i = 0; i < activeNodes.size(); i++)
	{
		ClipmapNode* node = activeNodes[i];

		viewNodes[i] = tree.allocator->alloc();
		viewNodes[i]->min = node->min_;
		viewNodes[i]->size = node->size_; 
		viewNodes[i]->mainMesh = node->renderMesh;
		viewNodes[i]->seamMesh = node->seamMesh;
	}

	std::sort(begin(viewNodes), end(viewNodes), 
		[](ClipmapViewNode* lhs, ClipmapViewNode* rhs)
		{
			return lhs->size < rhs->size;
		}
	);

	while (viewNodes.front()->size != viewNodes.back()->size)
	{
		// find the end of this run of equal sized viewNodes
		auto iter = begin(viewNodes);
		const int size = (*iter)->size;
		do
		{
			++iter;
		} 
		while ((*iter)->size == size);

		// construct the new parent nodes
		const std::vector<ClipmapViewNode*> runNodes(begin(viewNodes), iter);
		std::vector<ClipmapViewNode*> newNodes = ConstructClipmapViewNodeParents(tree.allocator, runNodes, rootMin);

		// set up for the next iteration: the newly created parents & any remaining input nodes
		newNodes.insert(end(newNodes), iter, end(viewNodes));
		viewNodes = newNodes;
	}

	int parentSize = viewNodes[0]->size * 2;
	while (viewNodes.size() > 1)
	{
		viewNodes = ConstructClipmapViewNodeParents(tree.allocator, viewNodes, rootMin);
		parentSize *= 2;
	}

	LVN_ASSERT(viewNodes.size() == 1);
	tree.root = viewNodes[0];
	return tree;
}

// ----------------------------------------------------------------------------

const vec3 ColourForMinLeafSize(const int minLeafSize)
{
	switch (minLeafSize)
	{
	case 1:
		return vec3(0.3f, 0.1f, 0.f);

	case 2:
		return vec3(0, 0.f, 0.5f);

	case 4:
		return vec3(0, 0.5f, 0.5f);

	case 8:
		return vec3(0.5f, 0.f, 0.5f);

	case 16:
		return vec3(0.0f, 0.5f, 0.f);

	default:
		return vec3(0.5f, 0.0f, 0.f);

	}
}

// ----------------------------------------------------------------------------

bool GenerateMeshDataForNode(
	Compute_MeshGenContext* meshGen,
	const char* const tag,
	const ivec3& min,
	const int clipmapNodeSize,
	MeshBuffer** meshBuffer,
	OctreeNode** seamNodes,
	int* numSeamNodes)
{
	rmt_ScopedCPUSample(GenerateMeshDataForNode);

	MeshBuffer* buffer = Render_AllocMeshBuffer(tag);
	if (!buffer)
	{
		printf("Error: unable to alloc mesh buffer\n");
		return false;
	}

	buffer->numTriangles = 0;
	buffer->numVertices = 0;

	std::vector<SeamNodeInfo> seamNodeInfo;
	const int error = meshGen->generateChunkMesh(min, clipmapNodeSize, buffer, seamNodeInfo);
	if (error < 0)
	{
		printf("Error generating mesh: %d\n", error);
		Render_FreeMeshBuffer(buffer);
		return false;
	}

	if (buffer->numTriangles > 0 || buffer->numVertices > 0)
	{
		*meshBuffer = buffer;

//		printf("gpu: %d triangles %d vertices\n",
//			buffer->numTriangles, buffer->numVertices);
	}
	else
	{
		Render_FreeMeshBuffer(buffer);
	}

	if (!seamNodeInfo.empty())
	{
		OctreeNode* nodeBuffer = new OctreeNode[seamNodeInfo.size()];
		const int seamNodeSize = clipmapNodeSize / meshGen->voxelsPerChunk();
		for (int i = 0; i < seamNodeInfo.size(); i++)
		{
			OctreeNode* seamNode = &nodeBuffer[i];
			const SeamNodeInfo& info = seamNodeInfo[i];

			seamNode->size = seamNodeSize;
			seamNode->min = ivec3(info.localspaceMin) * seamNodeSize + min;
			seamNode->type = Node_Leaf;
			seamNode->drawInfo = new OctreeDrawInfo;
			seamNode->drawInfo->averageNormal = vec3(info.normal);
			seamNode->drawInfo->position = vec3(info.position);
			seamNode->drawInfo->colour = ColourForMinLeafSize(clipmapNodeSize);
			seamNode->drawInfo->materialInfo = info.localspaceMin.w;
		}

		*seamNodes = nodeBuffer;
		*numSeamNodes = seamNodeInfo.size();
	}
	else
	{
		*seamNodes = nullptr;
		*numSeamNodes = 0;
	}

	return true;
}

// ----------------------------------------------------------------------------

int ConstructClipmapNodeData(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node,
	const float meshMaxError,
	const float meshMaxEdgeLen,
	const float meshMaxAngle)
{
	rmt_ScopedCPUSample(ConstructClipmapNode);

	LVN_ASSERT(!node->active_);
	LVN_ASSERT(!node->renderMesh)
	LVN_ASSERT(!node->seamMesh);
	LVN_ASSERT(!node->seamNodes);

	MeshBuffer* meshBuffer = nullptr;
	if (!GenerateMeshDataForNode(meshGen, "clipmap", 
		node->min_, node->size_, &meshBuffer, &node->seamNodes, &node->numSeamNodes))
	{
		node->active_ = false;
		return LVN_SUCCESS;
	}


	if (meshBuffer)
	{
		const vec4 centrePos = vec4(vec3(node->min_) + vec3(node->size_ / 2.f), 0.f);
		const float leafSize = LEAF_SIZE_SCALE * (node->size_ / CLIPMAP_LEAF_SIZE);

		MeshSimplificationOptions options;
		options.maxError = meshMaxError * leafSize;
		options.maxEdgeSize = meshMaxEdgeLen * leafSize;
		options.minAngleCosine = meshMaxAngle;

		ngMeshSimplifier(meshBuffer, centrePos, options);
		node->renderMesh = Render_AllocRenderMesh("clipmap", meshBuffer, vec3(centrePos));
	}

	node->active_ = node->numSeamNodes != 0 || node->renderMesh;
	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

MeshBuffer* ConstructCollisionNodeData(
	Compute_MeshGenContext* meshGen,
	ClipmapCollisionNode* node,
	const float meshMaxError,
	const float meshMaxEdgeLen,
	const float meshMaxAngle)
{
	rmt_ScopedCPUSample(ConstructCollisionNode);

	LVN_ASSERT(node->numSeamNodes == 0);
	LVN_ASSERT(!node->seamNodes);

	MeshBuffer* meshBuffer = nullptr;
	GenerateMeshDataForNode(meshGen, "collision", 
		node->min, COLLISION_NODE_SIZE, &meshBuffer, &node->seamNodes, &node->numSeamNodes);

	if (meshBuffer)
	{
		const vec4 centrePos = vec4(vec3(node->min) + vec3(COLLISION_NODE_SIZE / 2.f), 0.f);
		const float leafSize = LEAF_SIZE_SCALE * (COLLISION_NODE_SIZE / CLIPMAP_LEAF_SIZE);

		MeshSimplificationOptions options;
		options.maxError = meshMaxError * leafSize;
		options.maxEdgeSize = meshMaxEdgeLen * leafSize;
		options.minAngleCosine = meshMaxAngle;

		ngMeshSimplifier(meshBuffer, centrePos, options);
	}

	return meshBuffer;
}

// ----------------------------------------------------------------------------

bool FilterSeamNode(const int childIndex, const ivec3& seamBounds, const ivec3& min, const ivec3& max)
{
	switch (childIndex)
	{
	case 0:
		 return max.x == seamBounds.x || max.y == seamBounds.y || max.z == seamBounds.z;

	case 1:
		 return min.z == seamBounds.z;

	case 2:
		 return min.y == seamBounds.y;

	case 3:
		 return min.y == seamBounds.y || min.z == seamBounds.z;

	case 4:
		 return min.x == seamBounds.x;

	case 5:
		 return min.x == seamBounds.x || min.z == seamBounds.z;

	case 6:
		 return min.x == seamBounds.x || min.y == seamBounds.y;

	case 7:
		 return min == seamBounds;
	}

	return false;
}

// ----------------------------------------------------------------------------

void SelectSeamNodes(
	Compute_MeshGenContext* meshGen,
	const ivec3& min, 
	const int hostNodeSize, 
	const int neighbourNodeSize, 
	const int neighbourIndex,
	OctreeNode* seamNodes,
	const int numSeamNodes,
	std::vector<OctreeNode*>& selectedNodes)
{
	const ivec3 seamBounds = min + ivec3(hostNodeSize);
	const AABB aabb(min, hostNodeSize * 2);

	const int size = neighbourNodeSize / (meshGen->voxelsPerChunk() * LEAF_SIZE_SCALE);
	for (int j = 0; j < numSeamNodes; j++)
	{
		OctreeNode* node = &seamNodes[j];

		const auto max = node->min + ivec3(size * LEAF_SIZE_SCALE);
		if (!FilterSeamNode(neighbourIndex, seamBounds, node->min, max) ||
			!aabb.pointIsInside(node->min))
		{
			continue;
		}

		selectedNodes.push_back(node);
	}
}

// ----------------------------------------------------------------------------

void GenerateClipmapSeamMesh(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node, 
	const Clipmap& clipmap,
	const vec3& colour)
{
	rmt_ScopedCPUSample(GenClipmapSeamMesh);
	std::vector<OctreeNode*> seamNodes;
	seamNodes.reserve(2048);
	for (int i = 0; i < 8; i++)
	{
		const ivec3 neighbourMin = node->min_ + (CHILD_MIN_OFFSETS[i] * node->size_);
		ClipmapNode* candidateNeighbour = clipmap.findNode(neighbourMin, node->size_);
		if (!candidateNeighbour)
		{
			continue;
		}

		std::vector<ClipmapNode*> activeNodes = clipmap.findActiveNodes(candidateNeighbour);
		for (auto neighbourNode: activeNodes)
		{
			SelectSeamNodes(meshGen, node->min_, node->size_, neighbourNode->size_, i, 
				neighbourNode->seamNodes, neighbourNode->numSeamNodes, seamNodes);
		}
	}

	Octree seamOctree;
	Octree_ConstructUpwards(&seamOctree, seamNodes, node->min_, node->size_ * 2);
	OctreeNode* seamRoot = seamOctree.getRoot();

	LVN_ASSERT(!node->seamMesh);

	const int size = node->size_ / (meshGen->voxelsPerChunk() * LEAF_SIZE_SCALE);
	if (MeshBuffer* meshBuffer = Octree_GenerateMesh(seamRoot, colour))
	{
		const vec3 centrePos = vec3(seamRoot->min) + vec3(seamRoot->size / 2.f);
		node->seamMesh = Render_AllocRenderMesh("clipmap_seam", meshBuffer, centrePos);	
	}
}

// ----------------------------------------------------------------------------

MeshBuffer* GenerateCollisionSeamMesh(
	Compute_MeshGenContext* meshGen,
	ClipmapCollisionNode* node,
	const vec3& colour)
{
	LVN_ASSERT(node);

	std::vector<OctreeNode*> seamNodes;
	for (int i = 0; i < 8; i++)
	{
		const ivec3 neighbourMin = node->min + (COLLISION_NODE_SIZE * CHILD_MIN_OFFSETS[i]);
		const auto iter = g_clipmapCollisionNodes.find(neighbourMin);
		if (iter != end(g_clipmapCollisionNodes) && iter->second)
		{
			ClipmapCollisionNode* neighbourNode = iter->second;
			SelectSeamNodes(meshGen, node->min, COLLISION_NODE_SIZE, COLLISION_NODE_SIZE, i,
				neighbourNode->seamNodes, neighbourNode->numSeamNodes, seamNodes);
		}
	}

	Octree seamOctree;
	OctreeNode* seamRoot = Octree_ConstructUpwards(&seamOctree, seamNodes, node->min, COLLISION_NODE_SIZE * 2);

	const int size = COLLISION_NODE_SIZE / (meshGen->voxelsPerChunk() * LEAF_SIZE_SCALE);
	return Octree_GenerateMesh(seamRoot, colour);
}

// ----------------------------------------------------------------------------

void ReleaseClipmapNodeData(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node,
	std::vector<RenderMesh*>& invalidatedMeshes)
{
	node->active_ = false;

	meshGen->freeChunkOctree(node->min_, node->size_);

	if (node->renderMesh)
	{
		// collect the invalidated mesh indices so the meshes can be removed after
		// the replacement mesh(es) have been generated, which prevents flickering
		invalidatedMeshes.push_back(node->renderMesh);
		node->renderMesh = nullptr;
	}

	if (node->seamMesh)
	{
		invalidatedMeshes.push_back(node->seamMesh);
		node->seamMesh = nullptr;
	}

	for (int i = 0; i < node->numSeamNodes; i++)
	{
		OctreeNode* n = &node->seamNodes[i];
		delete n->drawInfo;
	}

	delete[] node->seamNodes;
	node->seamNodes = nullptr;
	node->numSeamNodes = 0;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

std::unordered_set<ClipmapNode*> g_allocatedNodes;

ClipmapNode* AllocClipmapNode()
{
	ClipmapNode* n = new ClipmapNode;
	g_allocatedNodes.insert(n);
	return n;
}

void FreeClipmapNode(ClipmapNode* n)
{
	LVN_ALWAYS_ASSERT("Unknown clipmap node!", g_allocatedNodes.find(n) != end(g_allocatedNodes));
	g_allocatedNodes.erase(n);
	delete n;
}

void TouchReachableNodes(ClipmapNode* n, std::vector<ClipmapNode*>& touchedNodes)
{
	if (!n)
	{
		return;
	}

	touchedNodes.push_back(n);

	for (int i = 0; i < 8; i++)
	{
		TouchReachableNodes(n->children_[i], touchedNodes);
	}
}

// ----------------------------------------------------------------------------

void ConstructChildren(ClipmapNode* node)
{
	if (node->size_ == CLIPMAP_LEAF_SIZE)
	{
		return;
	}

	for (int i = 0; i < 8; i++)
	{
		ClipmapNode* child = AllocClipmapNode();
		child->size_ = node->size_ / 2;
		child->min_ = node->min_ + (CHILD_MIN_OFFSETS[i] * child->size_);
		node->children_[i] = child;
	}

	for (int i = 0; i < 8; i++)
	{
		ConstructChildren(node->children_[i]);
	}
}

// ----------------------------------------------------------------------------

void CheckForEmptyNodes(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node, 
	const int emptyNodeSize)
{
	if (!node)
	{
		return;
	}

	if (node->size_ >= emptyNodeSize)
	{
		for (int i = 0; i < 8; i++)
		{
			CheckForEmptyNodes(meshGen, node->children_[i], emptyNodeSize);
		}
	}
	else if (node->size_ == emptyNodeSize)
	{
		if (int error = meshGen->isChunkEmpty(node->min_, emptyNodeSize, node->empty_))
		{
			printf("Error: isChunkEmpty call failed for [%d %d %d]\n", 
				node->min_.x, node->min_.y, node->min_.z);
		}
		
		if (node->empty_)
		{
			PropagateEmptyStateDownward(node);
		}
	}
}

// ----------------------------------------------------------------------------

void InsertEmptyCollisionNodes(ClipmapNode* node)
{
	if (!node)
	{
		return;
	}

	if (node->size_ > COLLISION_NODE_SIZE)
	{
		for (int i = 0; i < 8; i++)
		{
			InsertEmptyCollisionNodes(node->children_[i]);
		}
	}
	else if (node->size_ == COLLISION_NODE_SIZE)
	{
		if (node->empty_)
		{
			g_clipmapCollisionNodes[node->min_] = nullptr;
		}
	}
}

// ----------------------------------------------------------------------------

void Clipmap::constructTree()
{
	root_ = AllocClipmapNode();

	const ivec3 boundsSize = worldBounds_.max - worldBounds_.min;
	const int maxSize = glm::max(boundsSize.x, glm::max(boundsSize.y, boundsSize.z));

	int factor = maxSize / CLIPMAP_LEAF_SIZE;
	factor = 1 << glm::log2(factor);

	while ((factor * CLIPMAP_LEAF_SIZE) < maxSize)
	{
		factor *= 2;
	}

	root_->size_ = factor * CLIPMAP_LEAF_SIZE;

	const ivec3 boundsCentre = worldBounds_.min + (boundsSize / 2);
	root_->min_ = (boundsCentre - ivec3(root_->size_ / 2)) & ~(factor - 1);

	ConstructChildren(root_);
	CheckForEmptyNodes(physicsMeshGen_, root_, COLLISION_NODE_SIZE);
	PropagateEmptyStateUpward(root_, COLLISION_NODE_SIZE);
	InsertEmptyCollisionNodes(root_);

	std::vector<ivec3> collisionNodesToLoad;
	for (int x = worldBounds_.min.x; x < worldBounds_.max.x; x += COLLISION_NODE_SIZE)
	for (int y = worldBounds_.min.y; y < worldBounds_.max.y; y += COLLISION_NODE_SIZE)
	for (int z = worldBounds_.min.z; z < worldBounds_.max.z; z += COLLISION_NODE_SIZE)
	{
		const ivec3 min = ivec3(x, y, z);
		const auto iter = g_clipmapCollisionNodes.find(min);
		if (iter == end(g_clipmapCollisionNodes))
		{
			collisionNodesToLoad.push_back(min);
		}
	}

	// since we're initialising all the nodes need their seam generated
	loadCollisionNodes(collisionNodesToLoad, collisionNodesToLoad);
}

// ----------------------------------------------------------------------------

void Clipmap::initialise(
	const AABB& worldBounds)
{
	worldBounds_ = worldBounds;

	g_debugDrawBuffer = Render_AllocDebugDrawBuffer();
	g_clipmapCollisionNodeAllocator.initialise(MAX_COLLISION_NODES);

	clipmapMeshGen_ = Compute_MeshGenContext::create(CLIPMAP_VOXELS_PER_CHUNK); 
	physicsMeshGen_ = Compute_MeshGenContext::create(COLLISION_VOXELS_PER_CHUNK); 

	constructTree();
}

// ----------------------------------------------------------------------------

void DestroyClipmapNodes(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node, 
	std::vector<RenderMesh*>& invalidatedMeshes)
{
	if (!node)
	{
		return;
	}

	if (node->size_ > CLIPMAP_LEAF_SIZE)
	{
		for (int i = 0; i < 8; i++)
		{
			DestroyClipmapNodes(meshGen, node->children_[i], invalidatedMeshes);
			node->children_[i] = nullptr;
		}
	}

	ReleaseClipmapNodeData(meshGen, node, invalidatedMeshes);
	FreeClipmapNode(node);

	Render_FreeDebugDrawBuffer(&g_debugDrawBuffer);
}

// ----------------------------------------------------------------------------

void Clipmap::clear()
{
	std::vector<ClipmapNode*> touchedNodes;
	TouchReachableNodes(root_, touchedNodes);
	LVN_ALWAYS_ASSERT("Dangling nodes", touchedNodes.size() == g_allocatedNodes.size());

	std::vector<RenderMesh*> invalidatedMeshes;
	DestroyClipmapNodes(clipmapMeshGen_, root_, invalidatedMeshes);
	root_ = nullptr;

	g_updatePending = false;
	viewTree_.allocator = nullptr;
	viewTree_.root = nullptr;

	for (const auto iter: g_clipmapCollisionNodes)
	{
		ClipmapCollisionNode* node = iter.second;
		if (node)
		{
			ReleaseCollisionNode(node);
			Physics_UpdateWorldNodeMainMesh(node->min, nullptr);
			Physics_UpdateWorldNodeSeamMesh(node->min, nullptr);
		}
	}

	for (RenderMesh* mesh: invalidatedMeshes)
	{
		Render_FreeRenderMesh(&mesh);
	}

	g_clipmapCollisionNodeAllocator.clear();
	g_clipmapCollisionNodes.clear();
}

// ----------------------------------------------------------------------------

void FindVisibleNodes(
	ClipmapViewNode* node, 
	const Frustum& frustum, 
	std::vector<RenderMesh*>& meshes)
{
	if (!node)
	{
		return;
	}

	const AABB aabb(node->min, node->size);
	if (!AABBInsideFrustum(aabb, frustum))
	{
		return;
	}

	if (node->mainMesh)
	{
		meshes.push_back(node->mainMesh);
	}

	if (node->seamMesh)
	{
		meshes.push_back(node->seamMesh);
	}

	for (int i = 0; i < 8; i++)
	{
		FindVisibleNodes(node->children[i], frustum, meshes);
	}
}

// ----------------------------------------------------------------------------

void Clipmap::updateRenderState()
{
	if (!g_updatePending)
	{
		return;
	}

	viewTree_ = g_updateInfo.updatedTree;
	for (RenderMesh* mesh: g_updateInfo.invalidatedMeshes)
	{
		Render_FreeRenderMesh(&mesh);
	}

	g_updateInfo.invalidatedMeshes.clear();
	g_updatePending = false;
}

// ----------------------------------------------------------------------------

std::vector<RenderMesh*> Clipmap::findVisibleNodes(const Frustum& frustum) 
{
	std::vector<RenderMesh*> meshes;
	FindVisibleNodes(viewTree_.root, frustum, meshes);
	return meshes;
}

// ----------------------------------------------------------------------------

ClipmapNode* FindNode(ClipmapNode* node, const int size, const ivec3& min)
{
	if (!node)
	{
		return nullptr;
	}

	if (node->size_ == size && node->min_ == min)
	{
		return node;
	}

	const AABB bbox = AABB(node->min_, node->size_);
	if (bbox.pointIsInside(min))
	{
		for (int i = 0; i < 8; i++)
		{
			if (ClipmapNode* n = FindNode(node->children_[i], size, min))
			{
				return n;
			}
		}
	}

	return nullptr;
}

// ----------------------------------------------------------------------------

ClipmapNode* Clipmap::findNode(const ivec3& min, const int size) const
{
	return FindNode(root_, size, min);
}

// ----------------------------------------------------------------------------

void FindNodesOnPath(
	ClipmapNode* node, 
	const ivec3& endNodeMin, 
	const int endNodeSize, 
	std::vector<ClipmapNode*>& pathNodes)
{
	if (!node)
	{
		return;
	}

	const AABB bbox = AABB(node->min_, node->size_);
	if (!bbox.pointIsInside(endNodeMin))
	{
		return;
	}

	pathNodes.push_back(node);
	if (node->size_ > endNodeSize)
	{
		for (int i = 0; i < 8; i++)
		{
			FindNodesOnPath(node->children_[i], endNodeMin, endNodeSize, pathNodes);
		}
	}
}

// ----------------------------------------------------------------------------

std::vector<ClipmapNode*> Clipmap::findNodesOnPath(const ivec3& endNodeMin, const int endNodeSize)
{
	std::vector<ClipmapNode*> pathNodes;
	FindNodesOnPath(root_, endNodeMin, endNodeSize, pathNodes);
	return pathNodes;
}

// ----------------------------------------------------------------------------

float DistanceToNode(const ClipmapNode* node, const vec3& cameraPos)
{
#if 1
	// from http://stackoverflow.com/questions/5254838/calculating-distance-between-a-point-and-a-rectangular-box-nearest-point
	const vec3 min(node->min_);
	const vec3 max(node->min_ + ivec3(node->size_));
	const float dx = (float)glm::max(min.x - cameraPos.x, 0.f, cameraPos.x - max.x);
	const float dy = (float)glm::max(min.y - cameraPos.y, 0.f, cameraPos.y - max.y);
	const float dz = (float)glm::max(min.z - cameraPos.z, 0.f, cameraPos.z - max.z);
	return glm::sqrt(dx*dx + dy*dy + dz*dz);
#else
	float d = FLT_MAX;

	for (int i = 0; i < 8; i++)
	{
		const vec3 pos = vec3(node->min_ + (node->size_ * CHILD_MIN_OFFSETS[i]));
		const float nodeDistance = glm::length(pos - cameraPos);
		d = glm::min(nodeDistance, d);
	}

	const vec3 nodePos = vec3(node->min_ + (node->size_ / 2));
	const vec3 delta = nodePos - cameraPos;
	return glm::min(d, glm::length(delta));
#endif
}

// ----------------------------------------------------------------------------

void SelectActiveClipmapNodes(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node, 
	bool parentActive,
	const vec3& cameraPosition,
	std::vector<ClipmapNode*>& selectedNodes)
{
	if (!node)
	{
		return;
	}

	const AABB aabb = AABB(node->min_, node->size_);
	bool nodeActive = false;
	if (!parentActive && node->size_ <= LOD_MAX_NODE_SIZE)
	{
		const int size = node->size_ / (meshGen->voxelsPerChunk() * LEAF_SIZE_SCALE);
		const int distanceIndex = glm::log2(size);
		const float d = LOD_ACTIVE_DISTANCES[distanceIndex];
		const float nodeDistance = DistanceToNode(node, cameraPosition);

		if (nodeDistance >= d)
		{
			selectedNodes.push_back(node);
			nodeActive = true;
		}
	}

	if (node->active_ && !nodeActive)
	{
		node->invalidated_ = true;
	}

	for (int i = 0; i < 8; i++)
	{
		SelectActiveClipmapNodes(
			meshGen,
			node->children_[i], 
			parentActive || nodeActive,
			cameraPosition, 
			selectedNodes);
	}
}

// ----------------------------------------------------------------------------

void ReleaseInvalidatedNodes(
	Compute_MeshGenContext* meshGen,
	ClipmapNode* node, 
	std::vector<RenderMesh*>& invalidatedMeshes)
{
	if (!node)
	{
		return;
	}

	if (node->invalidated_)
	{
		ReleaseClipmapNodeData(meshGen, node, invalidatedMeshes);
		node->invalidated_ = false;
	}

	for (int i = 0; i < 8; i++)
	{
		ReleaseInvalidatedNodes(meshGen, node->children_[i], invalidatedMeshes);
	}
}

// ----------------------------------------------------------------------------

void FindCollisionNodes(ClipmapNode* node, std::vector<ClipmapNode*>& collisionNodes)
{
	if (!node)
	{
		return;
	}

	if (node->size_ == COLLISION_NODE_SIZE)
	{
		collisionNodes.push_back(node);
	}
	else if (node->size_ > COLLISION_NODE_SIZE)
	{
		for (int i = 0; i < 8; i++)
		{
			FindCollisionNodes(node->children_[i], collisionNodes);
		}
	}
}

// ----------------------------------------------------------------------------

void Clipmap::update(
	const glm::vec3& cameraPosition, 
	const Frustum& frustum)
{
	if (g_updatePending)
	{
		// previous update has not been processed yet
		return;
	}

	rmt_ScopedCPUSample(ClipmapUpdate);

	{
		rmt_ScopedCPUSample(queuedOperations);
		DeferredClipmapOperationQueue q = GetAllQueuedClipmapOperations();
		while (!q.empty())
		{
			DeferredClipmapOperation opFunc = q.front();
			q.pop_front();

			opFunc();
		}
	}

	std::vector<ClipmapNode*> selectedNodes;
	{
		rmt_ScopedCPUSample(SelectNodes);
		SelectActiveClipmapNodes(clipmapMeshGen_, root_, false, cameraPosition, selectedNodes);
	}

	// release the nodes invalidated due to not being active or an insert/remove
	std::vector<RenderMesh*> invalidatedMeshes;
	{
		rmt_ScopedCPUSample(ReleaseInvalidated);
		ReleaseInvalidatedNodes(clipmapMeshGen_, root_, invalidatedMeshes);
	}

	std::vector<ClipmapNode*> filteredNodes, reserveNodes, activeNodes;
	{
		rmt_ScopedCPUSample(FilterNodes);
		for (ClipmapNode* node: selectedNodes)
		{
			if (!node->active_ && !node->empty_)
			{
				const AABB aabb = AABB(node->min_, node->size_);
				if (AABBInsideFrustum(aabb, frustum))
				{
					filteredNodes.push_back(node);
				}
				else
				{
					reserveNodes.push_back(node);
				}
			}
			else
			{
				activeNodes.push_back(node);
			}
		}

		if (filteredNodes.empty())
		{
			// no nodes in the frustum need updated so update outside nodes 
			if (!reserveNodes.empty())
			{
				filteredNodes = reserveNodes;
			}
			else
			{
				// no nodes to update so no work to do
				return;
			}
		}
	}

	RenderDebugCmdBuffer renderCmds;
	std::vector<ClipmapNode*> emptyNodes;
	const auto& options = Options::get();

	// need to construct the all the nodes before attempting to select the seam nodes
	std::vector<ClipmapNode*> constructedNodes;
	for (ClipmapNode* node: filteredNodes)
	{
		if (int error = ConstructClipmapNodeData(clipmapMeshGen_, node, 
				options.meshMaxError_, options.meshMaxEdgeLen_, options.meshMinCosAngle_))
		{
			LVN_ASSERT(!node->renderMesh);
			LVN_ASSERT(!node->seamNodes);

			printf("Error constructing clipmap node data: %s (%d)\n", GetCLErrorString(error), error);
			continue;
		}

		if (node->renderMesh || node->numSeamNodes > 0)
		{
			constructedNodes.push_back(node);
			activeNodes.push_back(node);

			const vec3 colour = node->size_ == CLIPMAP_LEAF_SIZE ? RenderColour_Blue : RenderColour_Green;
	//		renderCmds.addCube(colour, 0.2f, vec3(node->min_), node->size_);
		}
		else
		{
			node->empty_ = true;
			emptyNodes.push_back(node);
		}
	}

	for (ClipmapNode* node: emptyNodes)
	{
		PropagateEmptyStateDownward(node);
	}

	if (false)
	{
		std::vector<ClipmapNode*> emptyNodes;
		FindEmptyNodes(root_, emptyNodes);
		
		for (ClipmapNode* node: emptyNodes)
		{
			const int size = node->size_ / (clipmapMeshGen_->voxelsPerChunk() * LEAF_SIZE_SCALE);
			renderCmds.addCube(ColourForMinLeafSize(size), 0.2f, vec3(node->min_), node->size_);
		}
	}

	if (g_debugDrawBuffer != -1)
	{
		Render_SetDebugDrawCmds(g_debugDrawBuffer, renderCmds);
	}

	std::unordered_set<ClipmapNode*> seamUpdateNodes;
	{
		rmt_ScopedCPUSample(findSeamNodes);

		for (ClipmapNode* node: constructedNodes)
		{
			// setting a node will invalidate its seam, so need to tell neighbours to update too
			// note the range is [0,7] so we include the node itself as requiring an update
			for (int i = 0; i < 8; i++)
			{
				const ivec3 neighbourMin = node->min_ - (CHILD_MIN_OFFSETS[i] * node->size_);
				if (ClipmapNode* candidateNeighbour = findNode(neighbourMin, node->size_))
				{
					std::vector<ClipmapNode*> activeNodes = findActiveNodes(candidateNeighbour);
					seamUpdateNodes.insert(begin(activeNodes), end(activeNodes));
				}
			}
		}
	}

	const vec3 colour = RandomColour();
	for (ClipmapNode* n: seamUpdateNodes)
	{
		if (n->seamMesh)
		{
			invalidatedMeshes.push_back(n->seamMesh);
			n->seamMesh = nullptr;
		}

		GenerateClipmapSeamMesh(clipmapMeshGen_, n, *this, colour);
	}

	g_updateInfo.updatedTree = ConstructClipmapViewTree(activeNodes, root_->min_); 
	g_updateInfo.invalidatedMeshes = invalidatedMeshes;

	g_updatePending = true;
}

// ----------------------------------------------------------------------------

void Clipmap::loadCollisionNodes(
	const std::vector<ivec3>& requestedNodes,
	const std::vector<glm::ivec3>& requestedSeamNodes)
{
	rmt_ScopedCPUSample(loadCollisionNodes);

	std::vector<ClipmapCollisionNode*> constructedNodes;
	for (const ivec3& min: requestedNodes)
	{
		rmt_ScopedCPUSample(ProcessNode);

		const auto iter = g_clipmapCollisionNodes.find(min);
		if (iter != end(g_clipmapCollisionNodes))
		{
			ClipmapCollisionNode* node = iter->second;
			ReleaseCollisionNode(node);
			g_clipmapCollisionNodeAllocator.free(node);
		}

		ClipmapCollisionNode* node = g_clipmapCollisionNodeAllocator.alloc();
		*node = ClipmapCollisionNode(min);

		const auto& options = Options::get();
		MeshBuffer* mainMesh = ConstructCollisionNodeData(physicsMeshGen_, node,
			options.meshMaxError_, options.meshMaxEdgeLen_, options.meshMinCosAngle_);
		if (!mainMesh)
		{
			// use nullptr to represent an empty field -- maybe use a specific instance instead?
			g_clipmapCollisionNodes[min] = nullptr;
			g_clipmapCollisionNodeAllocator.free(node);
			continue;
		}

		g_clipmapCollisionNodes[min] = node;
		Physics_UpdateWorldNodeMainMesh(node->min, mainMesh);

	//	printf("Constructed collision node [%d %d %d]\n", 
	//		node->min.x, node->min.y, node->min.z);
		constructedNodes.push_back(node);
	}

	{
		rmt_ScopedCPUSample(GenerateCollisionSeam);
		const vec3 colour = RandomColour();
		for (const ivec3& min: requestedSeamNodes)
		{
			const auto iter = g_clipmapCollisionNodes.find(min);
			LVN_ASSERT(iter != end(g_clipmapCollisionNodes));
			if (ClipmapCollisionNode* node = iter->second)
			{
				MeshBuffer* seamMesh = GenerateCollisionSeamMesh(physicsMeshGen_, node, RandomColour());
				if (seamMesh && seamMesh->numTriangles > 0)
				{
					Physics_UpdateWorldNodeSeamMesh(node->min, seamMesh);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------

ClipmapNode* FindNodeContainingChunk(ClipmapNode* node, const ivec3& min)
{
	if (!node)
	{
		return nullptr;
	}

	const AABB nodeBB(node->min_, node->size_);
	if (!nodeBB.pointIsInside(min))
	{
		return nullptr;
	}

	if (node->active_)
	{
		// this is the node responsible for drawing the chunk
		return node;
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			if (auto ptr = FindNodeContainingChunk(node->children_[i], min))
			{
				return ptr;
			}
		}

		return nullptr;
	}
}

// ----------------------------------------------------------------------------

ClipmapNode* Clipmap::findNodeContainingChunk(const ivec3& min) const
{
	return FindNodeContainingChunk(root_, min);
}

// ----------------------------------------------------------------------------

void FindActiveNodes(ClipmapNode* node, const ClipmapNode* referenceNode, std::vector<ClipmapNode*>& activeNodes)
{
	if (!node || !referenceNode)
	{
		return;
	}

	const AABB bbox = AABB(node->min_, node->size_);
	const AABB referenceBBox = AABB(referenceNode->min_, referenceNode->size_);
	if (bbox.pointIsInside(referenceNode->min_) || referenceBBox.pointIsInside(node->min_))
	{
		if (node->active_)
		{
			activeNodes.push_back(node);
		}
		else if (node->size_ > CLIPMAP_LEAF_SIZE)
		{
			for (int i = 0; i < 8; i++)
			{
				FindActiveNodes(node->children_[i], referenceNode, activeNodes);
			}
		}
	}
}

// ----------------------------------------------------------------------------

std::vector<ClipmapNode*> Clipmap::findActiveNodes(const ClipmapNode* node) const
{
	std::vector<ClipmapNode*> activeNodes;
	FindActiveNodes(root_, node, activeNodes);
	return activeNodes;
}

// ----------------------------------------------------------------------------

ClipmapNode* FindNodeForChunk(ClipmapNode* node, const ivec3& min)
{
	if (!node)
	{
		return nullptr;
	}

	// i.e. the node is active
	const AABB bbox = AABB(node->min_, node->size_);
	if (bbox.pointIsInside(min))
	{
		if (node->size_ == CLIPMAP_LEAF_SIZE)
		{
			// i.e. the node is active
			return node;
		}
		else 
		{
			for (int i = 0; i < 8; i++)
			{
				if (ClipmapNode* child = FindNodeForChunk(node->children_[i], min))
				{
					return child;
				}
			}
		}
	}

	return nullptr;
}

// ----------------------------------------------------------------------------

ClipmapNode* Clipmap::findNodeForChunk(const glm::ivec3& min) const
{
	return FindNodeForChunk(root_, min);
}

// ----------------------------------------------------------------------------

void FindIntersectingCollisionVolumes(
	ClipmapNode* node, 
	const vec3& rayOrigin, 
	const vec3& rayDir,
	std::vector<ivec3>& volumes,
	std::vector<vec3>& intersectPositions)
{
	if (!node)
	{
		return;
	}

	const AABB aabb(node->min_, node->size_);
	vec3 intersectPoint;
	if (!aabb.intersect(rayOrigin, rayDir, &intersectPoint))
	{
		return;
	}

	if (node->size_ == COLLISION_NODE_SIZE)
	{
		volumes.push_back(node->min_);
		intersectPositions.push_back(intersectPoint);
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			FindIntersectingCollisionVolumes(node->children_[i], rayOrigin, rayDir, volumes, intersectPositions);
		}
	}
}

// ----------------------------------------------------------------------------

void Clipmap::findIntersectingCollisionVolumes(
	const vec3& rayOrigin, 
	const vec3& rayDir,
	std::vector<ivec3>& volumes,
	std::vector<vec3>& intersectPositions)
{
	FindIntersectingCollisionVolumes(root_, rayOrigin, rayDir, volumes, intersectPositions);
}

// ----------------------------------------------------------------------------

void FindNodesInsideAABB(
	ClipmapNode* node, 
	const AABB& aabb, 
	std::vector<ClipmapNode*>& nodes)
{
	if (!node)
	{
		return;
	}

	const AABB nodeBB(node->min_, node->size_);
	if (!aabb.overlaps(nodeBB))
	{
		return;
	}

	for (int i = 0; i < 8; i++)
	{
		FindNodesInsideAABB(node->children_[i], aabb, nodes);
	}

	// traversal order is arbitrary
	if (node->size_ <= LOD_MAX_NODE_SIZE)
	{
		nodes.push_back(node);
	}
}

// ----------------------------------------------------------------------------

std::vector<ClipmapNode*> Clipmap::findNodesInsideAABB(
	const AABB& aabb) const
{
	std::vector<ClipmapNode*> nodes;
	FindNodesInsideAABB(root_, aabb, nodes);
	return nodes;
}

// ----------------------------------------------------------------------------

static std::vector<CSGOperationInfo> g_operationQueue;
static std::mutex g_operationMutex;
const vec3 CSG_OFFSET(0.5f);
const ivec3 CSG_BOUNDS_FUDGE(2);

// ----------------------------------------------------------------------------

void Clipmap::queueCSGOperation(
	const vec3& origin, 
	const glm::vec3& brushSize, 
	const RenderShape brushShape,
	const int brushMaterial, 
	const bool isAddOperation)
{
	rmt_ScopedCPUSample(QueueCSGOperation);

	CSGOperationInfo opInfo;
	opInfo.origin = vec4((origin / (float)LEAF_SIZE_SCALE) + CSG_OFFSET, 0.f);
	opInfo.dimensions = vec4(brushSize / 2.f, 0.f) / (float)LEAF_SIZE_SCALE;
	opInfo.brushShape = brushShape;
	opInfo.material = isAddOperation ? brushMaterial : MATERIAL_AIR;
	opInfo.type = isAddOperation ? 0 : 1;

	std::lock_guard<std::mutex> lock(g_operationMutex);
	g_operationQueue.push_back(opInfo);
}

AABB CalcCSGOperationBounds(const CSGOperationInfo& opInfo)
{
	const ivec3 boundsHalfSize = ivec3(opInfo.dimensions * LEAF_SIZE_SCALE) + CSG_BOUNDS_FUDGE;
	const ivec3 scaledOrigin = ivec3((vec3(opInfo.origin) - vec3(CSG_OFFSET)) * (float)LEAF_SIZE_SCALE);
	return AABB(scaledOrigin - boundsHalfSize, scaledOrigin + boundsHalfSize);
}

// ----------------------------------------------------------------------------

void Clipmap::processCSGOperationsImpl()
{
	rmt_ScopedCPUSample(ProcessCSGOps);

	std::vector<CSGOperationInfo> operations;
	{
		std::lock_guard<std::mutex> lock(g_operationMutex);
		operations = g_operationQueue;
		g_operationQueue.clear();
	}

	if (operations.empty())
	{
		return;
	}

//	printf("%d operations\n", operations.size());

	std::unordered_set<ClipmapNode*> touchedNodes;
	for (const CSGOperationInfo& opInfo: operations)
	{
		std::vector<ClipmapNode*> opNodes = findNodesInsideAABB(CalcCSGOperationBounds(opInfo));
		for (ClipmapNode* node: opNodes)
		{
			touchedNodes.insert(node);
		}
	}

	std::vector<ivec3> touchedCollisionNodes;
	for (auto clipmapNode: touchedNodes)
	{
		rmt_ScopedCPUSample(processNode);
		if (clipmapNode->size_ == COLLISION_NODE_SIZE)
		{
			const ivec3 collisionNodeMin = clipmapNode->min_ & ~(COLLISION_NODE_SIZE - 1);
			if (std::find(begin(touchedCollisionNodes), end(touchedCollisionNodes), collisionNodeMin) 
				== end(touchedCollisionNodes))
			{
				touchedCollisionNodes.push_back(collisionNodeMin);
			}

			if (int error = 
				physicsMeshGen_->applyCSGOperations(operations, clipmapNode->min_, COLLISION_NODE_SIZE) < 0)
			{
				printf("Error! Compute_ApplyCSGOperation failed: %s\n", GetCLErrorString(error));
				exit(EXIT_FAILURE);
			}

			// free the current octree to force a reconstruction
			physicsMeshGen_->freeChunkOctree(clipmapNode->min_, clipmapNode->size_);
		}

		if (clipmapNode->active_)
		{
			if (int error = 
				clipmapMeshGen_->applyCSGOperations(operations, clipmapNode->min_, clipmapNode->size_) < 0)
			{
				printf("Error! Compute_ApplyCSGOperation failed: %s\n", GetCLErrorString(error));
				exit(EXIT_FAILURE);
			}
		}
		 
		// free the current octree to force a reconstruction
		clipmapMeshGen_->freeChunkOctree(clipmapNode->min_, clipmapNode->size_);

		clipmapNode->invalidated_ = true;
		clipmapNode->empty_ = false;
	}

	for (const auto& opInfo: operations)
	{
		Compute_StoreCSGOperation(opInfo, CalcCSGOperationBounds(opInfo));
	}

	std::vector<ivec3> touchedSeamNodes;
	for (const ivec3& min: touchedCollisionNodes)
	{
		// start at 1 to avoid checking the 'host' node
		for (int i = 1; i < 8; i++)
		{
			// we know the operation touched the node at min so we can determine if another node
			// should regenerate its seam if it was also touched by the operation (i.e. the 
			// operation stradles both nodes)
			const ivec3 neighbourMin = min - (CHILD_MIN_OFFSETS[i] * COLLISION_NODE_SIZE);
			const auto iter = std::find(begin(touchedCollisionNodes), end(touchedCollisionNodes), neighbourMin);
			if (iter != end(touchedCollisionNodes))
			{
				if (std::find(begin(touchedSeamNodes), end(touchedSeamNodes), neighbourMin) 
					== end(touchedSeamNodes))
				{
					touchedSeamNodes.push_back(neighbourMin);
				}
			}
		}
	}

	loadCollisionNodes(touchedCollisionNodes, touchedSeamNodes);
}

// ----------------------------------------------------------------------------

void Clipmap::processCSGOperations()
{
	// TODO use this method of queue-ing/defering once clipmap operations are threaded
//	EnqueueClipmapOperation(std::bind(&Clipmap::processCSGOperationsImpl, this));
	processCSGOperationsImpl();
}

