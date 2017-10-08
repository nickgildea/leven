#ifndef		HAS_OCTREE_H_BEEN_INCLUDED
#define		HAS_OCTREE_H_BEEN_INCLUDED

#include	"volume_constants.h"
#include	"render_types.h"
#include	"volume_materials.h"
#include	"slab_allocator.h"

#include	<stdint.h>
#include	<vector>
#include	<memory>
#include	<functional>

#include	<glm/glm.hpp>

struct OctreeDrawInfo;
class OctreeNode;
class Octree;

// ----------------------------------------------------------------------------

OctreeNode* Octree_ConstructUpwards(
	Octree* octree,
	const std::vector<OctreeNode*>& inputNodes, 
	const glm::ivec3& rootMin,
	const int rootNodeSize
);

// ----------------------------------------------------------------------------

MeshBuffer* Octree_GenerateMesh(
	OctreeNode* root, 
	const glm::vec3& colour
);

// ----------------------------------------------------------------------------

enum OctreeNodeType
{
	Node_None,

	Node_Internal,
	Node_Leaf,
};

// ----------------------------------------------------------------------------

struct OctreeDrawInfo 
{
	OctreeDrawInfo()
		: index(-1)
		, colour(1.f)
		, materialInfo(0)
		, qefIndex(-1)
		, error(-1.f)
	{
	}

	
	int				index;		// TODO should be unsigned int? or unsigned short?
	glm::vec3		position;
	glm::vec3		averageNormal;
	glm::vec3		colour;
	int				materialInfo;
	int				qefIndex;	// TODO remove qefIndex as its only required temporarily
	float			error;
};

// ----------------------------------------------------------------------------

class OctreeNode
{
public:

	OctreeNode()
		: type(Node_None)
		, min(0, 0, 0)
		, size(0)
		, drawInfo(nullptr)
	{
		for (int i = 0; i < 8; i++)
		{
			children[i] = nullptr;
		}
	}

	OctreeNodeType getType() const
	{
		return type; 
	}

	OctreeNodeType				type;
	glm::ivec3					min;
	int							size;
	OctreeNode*					children[8];
	OctreeDrawInfo*				drawInfo;
};

// ----------------------------------------------------------------------------

class Octree
{
public:

	Octree()
		: root_(nullptr)
	{
	}

	OctreeNode*	allocNode()
	{
		return nodeAlloc_.alloc();
	}

	OctreeDrawInfo* allocDrawInfo()
	{
		return drawInfoAlloc_.alloc();
	}

	void clear()
	{
		nodeAlloc_.clear();
		drawInfoAlloc_.clear();
		root_ = nullptr;
	}

	void setRoot(OctreeNode* root)
	{
		root_ = root;
	}

	OctreeNode* getRoot() const
	{
		return root_;
	}

	size_t allocSize() const
	{
		return 
			(nodeAlloc_.size() * sizeof(OctreeNode)) +
			(drawInfoAlloc_.size() * sizeof(OctreeDrawInfo));
	}

	size_t nodeCount() const
	{
		return nodeAlloc_.size();
	}

private:

	typedef SlabAllocator<OctreeNode, 512> NodeAlloc;
	typedef SlabAllocator<OctreeDrawInfo, 512> DrawInfoAlloc;

	// make non-copyable
	Octree(const Octree&);
	Octree& operator=(const Octree&);

	OctreeNode*						root_;
	NodeAlloc						nodeAlloc_;
	DrawInfoAlloc					drawInfoAlloc_;
};

#endif //	HAS_OCTREE_H_BEEN_INCLUDED
