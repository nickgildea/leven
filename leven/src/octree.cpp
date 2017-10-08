#include	"octree.h"
#include	"aabb.h"
#include	"volume_constants.h"
#include	"contour_constants.h"
#include	"compute.h"
#include	"ng_mesh_simplify.h"
#include	"timer.h"
#include	"volume.h"
#include	"cuckoo.h"
#include	"render.h"
#include	"glm_hash.h"

#include	<algorithm>
#include	<unordered_map>
#include	<glm/ext.hpp>

using		glm::ivec3;
using		glm::vec3;
using		glm::vec4;

// ----------------------------------------------------------------------------

uint64_t HashOctreeMin(const ivec3& min)
{
	return min.x | ((uint64_t)min.y << 20) | ((uint64_t)min.z << 40);
}

// ----------------------------------------------------------------------------

std::vector<OctreeNode*> ConstructParents(
	Octree* octree,
	const std::vector<OctreeNode*>& nodes, 
	const int parentSize, 
	const ivec3& rootMin
	)
{
	std::unordered_map<uint64_t, OctreeNode*> parentsHashmap;

	std::for_each(begin(nodes), end(nodes), [&](OctreeNode* node)
	{
		// because the octree is regular we can calculate the parent min
		const ivec3 localPos = (node->min - rootMin);
		const ivec3 parentPos = node->min - (localPos % parentSize);
		
		const uint64_t parentIndex = HashOctreeMin(parentPos - rootMin);
		OctreeNode* parentNode = nullptr;

		auto iter =  parentsHashmap.find(parentIndex);
		if (iter == end(parentsHashmap))
		{
			parentNode = octree->allocNode();
			parentNode->type = Node_Internal;
			parentNode->min = parentPos;
			parentNode->size = parentSize;

			parentsHashmap.insert(std::pair<uint64_t, OctreeNode*>(parentIndex, parentNode));
		}
		else
		{
			parentNode = iter->second;
		}

		bool foundParentNode = false;
		for (int i = 0; i < 8; i++)
		{
			const ivec3 childPos = parentPos + ((parentSize / 2) * CHILD_MIN_OFFSETS[i]);
			if (childPos == node->min)
			{
//				LVN_ALWAYS_ASSERT("Duplicate node", parentNode->children[i] == nullptr);
				parentNode->children[i] = node;
				foundParentNode = true;
				break;
			}
		}

		LVN_ALWAYS_ASSERT("No parent node found", foundParentNode);
	});

	std::vector<OctreeNode*> parents;
	std::for_each(begin(parentsHashmap), end(parentsHashmap), [&](std::pair<uint64_t, OctreeNode*> pair)
	{
		parents.push_back(pair.second);
	});

	return parents;
}

// ----------------------------------------------------------------------------

OctreeNode* Octree_ConstructUpwards(
	Octree* octree,
	const std::vector<OctreeNode*>& inputNodes, 
	const ivec3& rootMin,
	const int rootNodeSize
	)
{
	if (inputNodes.empty())
	{
		return nullptr;
	}

	std::vector<OctreeNode*> nodes(begin(inputNodes), end(inputNodes));
	std::sort(begin(nodes), end(nodes), 
		[](OctreeNode*& lhs, OctreeNode*& rhs)
		{
			return lhs->size < rhs->size;
		});

	// the input nodes may be different sizes if a seam octree is being constructed
	// in that case we need to process the input nodes in stages along with the newly
	// constructed parent nodes until the all the nodes have the same size
	while (nodes.front()->size != nodes.back()->size)
	{
		// find the end of this run
		auto iter = begin(nodes);
		int size = (*iter)->size;
		do
		{
			++iter;
		} while ((*iter)->size == size);

		// construct the new parent nodes for this run
		std::vector<OctreeNode*> newNodes(begin(nodes), iter);
		newNodes = ConstructParents(octree, newNodes, size * 2, rootMin);

		// set up for the next iteration: the parents produced plus any remaining input nodes
		newNodes.insert(end(newNodes), iter, end(nodes));
		std::swap(nodes, newNodes);
	}

	int parentSize = nodes.front()->size * 2;
	while (parentSize <= rootNodeSize)
	{
		nodes = ConstructParents(octree, nodes, parentSize, rootMin);
		parentSize *= 2;
	}

	LVN_ALWAYS_ASSERT("There can be only one! (root node)", nodes.size() == 1);
	OctreeNode* root = nodes.front();
	LVN_ASSERT(root);

	LVN_ASSERT(root->min.x == rootMin.x);
	LVN_ASSERT(root->min.y == rootMin.y);
	LVN_ASSERT(root->min.z == rootMin.z);

	octree->setRoot(root);
	return root;
}

// ----------------------------------------------------------------------------

void CloneNode(Octree* clonedOctree, const OctreeNode* sourceNode, OctreeNode* node)
{
	if (!clonedOctree || !sourceNode || !node)
	{
		return;
	}

	node->type = sourceNode->type;
	node->min = sourceNode->min;
	node->size = sourceNode->size;

	if (sourceNode->drawInfo)
	{
		node->drawInfo = clonedOctree->allocDrawInfo();
		node->drawInfo->position = sourceNode->drawInfo->position;
		node->drawInfo->averageNormal = sourceNode->drawInfo->averageNormal;
		node->drawInfo->error = sourceNode->drawInfo->error;
		node->drawInfo->materialInfo = sourceNode->drawInfo->materialInfo;
		node->drawInfo->colour = sourceNode->drawInfo->colour;
	}

	for (int i = 0; i < 8; i++)
	{
		if (!sourceNode->children[i])
		{
			continue;
		}

		node->children[i] = clonedOctree->allocNode();
		CloneNode(clonedOctree, sourceNode->children[i], node->children[i]);
	}
}

// ----------------------------------------------------------------------------

void Octree_Clone(Octree* clone, const Octree* source)
{
	OctreeNode* root = clone->allocNode();
	clone->setRoot(root);

	CloneNode(clone, source->getRoot(), root);
}

// ----------------------------------------------------------------------------

void GenerateVertexIndices(OctreeNode*& node, const vec3& colour, MeshBuffer* buffer)
{
	if (!node)
	{
		return;
	}

	bool hasChildren = false;
	for (int i = 0; i < 8; i++)
	{
		hasChildren |= node->children[i] != nullptr;
	}

	if (hasChildren)
	{
		for (int i = 0; i < 8; i++)
		{
			GenerateVertexIndices(node->children[i], colour, buffer);
		}
	}
	else
	{
		OctreeDrawInfo* d = node->drawInfo;
		if (!d)
		{
			printf("Error! Could not add vertex!\n");
			exit(EXIT_FAILURE);
		}

		d->index = buffer->numVertices;
		buffer->vertices[buffer->numVertices++] = 
			MeshVertex(vec4(d->position, 0.f), vec4(d->averageNormal, 0.f), 
			vec4(colour, (float)(d->materialInfo >> 8)));
	}
}

// ----------------------------------------------------------------------------

void ContourProcessEdge(OctreeNode* node[4], int dir, MeshBuffer* buffer)
{
	int minSize = INT_MAX;
	int minIndex = 0;
	int indices[4] = { -1, -1, -1, -1 };
	bool flip = false;
	bool signChange[4] = { false, false, false, false };

	for (int i = 0; i < 4; i++)
	{
		if (node[i]->type != Node_Internal)
		{
			const int edge = processEdgeMask[dir][i];
			const int c0 = edgevmap[edge][0];
			const int c1 = edgevmap[edge][1];
			const int corners = node[i]->drawInfo->materialInfo & 0xff;
			const int m0 = (corners >> c0) & 1;
			const int m1 = (corners >> c1) & 1;

			if (node[i]->size < minSize)
			{
				minSize = node[i]->size;
				minIndex = i;
				flip = m1 != 1; 
			}

			indices[i] = node[i]->drawInfo->index;
			signChange[i] = (m0 && !m1) || (!m0 && m1);
		}
	}

	if (!signChange[minIndex])
	{
		return;
	}

	if (!flip)
	{
		buffer->triangles[buffer->numTriangles++] = MeshTriangle(indices[0], indices[1], indices[3]);
		buffer->triangles[buffer->numTriangles++] = MeshTriangle(indices[0], indices[3], indices[2]);
	}
	else
	{
		buffer->triangles[buffer->numTriangles++] = MeshTriangle(indices[0], indices[3], indices[1]);
		buffer->triangles[buffer->numTriangles++] = MeshTriangle(indices[0], indices[2], indices[3]);
	}
}

// ----------------------------------------------------------------------------

void ContourEdgeProc(OctreeNode* node[4], int dir, MeshBuffer* buffer)
{
	if (!node[0] || !node[1] || !node[2] || !node[3])
	{
		return;
	}

	std::unordered_set<ivec3> chunks;
	for (int i = 0; i < 4; i++)
	{
		chunks.insert(ChunkMinForPosition(node[i]->min));
	}

	// bit of a hack but it works: prevent overlapping seams by only
	// processing edges that stradle multiple chunks
	if (chunks.size() == 1)
	{
		return;
	}

	const bool isBranch[4] = 
	{
		node[0]->type == Node_Internal,
		node[1]->type == Node_Internal,
		node[2]->type == Node_Internal,
		node[3]->type == Node_Internal,
	};

	if (!isBranch[0] && !isBranch[1] && !isBranch[2] && !isBranch[3])
	{
		ContourProcessEdge(node, dir, buffer);
	}
	else
	{
		for (int i = 0; i < 2; i++)
		{
			OctreeNode* edgeNodes[4];
			const int c[4] = 
			{
				edgeProcEdgeMask[dir][i][0],
				edgeProcEdgeMask[dir][i][1],
				edgeProcEdgeMask[dir][i][2],
				edgeProcEdgeMask[dir][i][3],
			};

			for (int j = 0; j < 4; j++)
			{
				if (!isBranch[j])
				{
					edgeNodes[j] = node[j];
				}
				else
				{
					edgeNodes[j] = node[j]->children[c[j]];
				}
			}

			ContourEdgeProc(edgeNodes, edgeProcEdgeMask[dir][i][4], buffer);
		}
	}
}

// ----------------------------------------------------------------------------

void ContourFaceProc(OctreeNode* node[2], int dir, MeshBuffer* buffer)
{
	if (!node[0] || !node[1])
	{
		return;
	}

	// bit of a hack but it works: prevent overlapping seams by only
	// processing edges that stradle multiple chunks
	if (ChunkMinForPosition(node[0]->min) == ChunkMinForPosition(node[1]->min))
	{
		return;
	}

	const bool isBranch[2] = 
	{
		node[0]->type == Node_Internal,
		node[1]->type == Node_Internal,
	};

	if (isBranch[0] || isBranch[1])
	{
		for (int i = 0; i < 4; i++)
		{
			OctreeNode* faceNodes[2];
			const int c[2] = 
			{
				faceProcFaceMask[dir][i][0], 
				faceProcFaceMask[dir][i][1], 
			};

			for (int j = 0; j < 2; j++)
			{
				if (!isBranch[j])
				{
					faceNodes[j] = node[j];
				}
				else
				{
					faceNodes[j] = node[j]->children[c[j]];
				}
			}

			ContourFaceProc(faceNodes, faceProcFaceMask[dir][i][2], buffer);
		}
		
		const int orders[2][4] =
		{
			{ 0, 0, 1, 1 },
			{ 0, 1, 0, 1 },
		};
		for (int i = 0; i < 4; i++)
		{
			OctreeNode* edgeNodes[4];
			const int c[4] =
			{
				faceProcEdgeMask[dir][i][1],
				faceProcEdgeMask[dir][i][2],
				faceProcEdgeMask[dir][i][3],
				faceProcEdgeMask[dir][i][4],
			};

			const int* order = orders[faceProcEdgeMask[dir][i][0]];
			for (int j = 0; j < 4; j++)
			{
			//	if (node[order[j]]->getType() == Node_Leaf ||
			//		node[order[j]]->getType() == Node_Psuedo)
				if (!isBranch[order[j]])
				{
					edgeNodes[j] = node[order[j]];
				}
				else
				{
					edgeNodes[j] = node[order[j]]->children[c[j]];
				}
			}

			ContourEdgeProc(edgeNodes, faceProcEdgeMask[dir][i][5], buffer);
		}
	}
}

// ----------------------------------------------------------------------------

void ContourCellProc(OctreeNode* node, MeshBuffer* buffer)
{
	if (node == NULL || node->type == Node_Leaf)
	{
		return;
	}

	for (int i = 0; i < 8; i++)
	{
		ContourCellProc(node->children[i], buffer);
	}

	for (int i = 0; i < 12; i++)
	{
		OctreeNode* faceNodes[2];
		const int c[2] = { cellProcFaceMask[i][0], cellProcFaceMask[i][1] };
		
		faceNodes[0] = node->children[c[0]];
		faceNodes[1] = node->children[c[1]];

		ContourFaceProc(faceNodes, cellProcFaceMask[i][2], buffer);
	}

	for (int i = 0; i < 6; i++)
	{
		OctreeNode* edgeNodes[4];
		const int c[4] = 
		{
			cellProcEdgeMask[i][0],
			cellProcEdgeMask[i][1],
			cellProcEdgeMask[i][2],
			cellProcEdgeMask[i][3],
		};

		for (int j = 0; j < 4; j++)
		{
			edgeNodes[j] = node->children[c[j]];
		}

		ContourEdgeProc(edgeNodes, cellProcEdgeMask[i][4], buffer);
	}
}

// ----------------------------------------------------------------------------

#if ENABLE_DUMP_MESH
void DumpMesh(const ivec3& min, MeshBuffer* buffer)
{
	const int id = HashOctreeMin(min);
	const vec3 offset(min);

	char filename[1024];
	_snprintf(filename, 1023, "mesh_%016I64x", id);

	FILE* f = fopen(filename, "wb");
	if (!f)
	{
		printf("Couldn't open file '%s' for writing\n", filename);
		return;
	}

	fwrite(&buffer->numVertices_, sizeof(int), 1, f);
	fwrite(&buffer->numTriangles, sizeof(int), 1, f);

	for (int i = 0; i < buffer->numVertices_; i++)
	{
		MeshVertex copy = buffer->vertices_[i];
		copy.xyz -= offset;
		fwrite(&copy, sizeof(MeshVertex), 1, f);
	}

	for (int i = 0; i < buffer->numTriangles; i++)
	{
		fwrite(&buffer->triangles[i], sizeof(MeshTriangle), 1, f);
	}

	fclose(f);
}
#endif

// ----------------------------------------------------------------------------

MeshBuffer* Octree_GenerateMesh(
	OctreeNode* root, 
	const vec3& colour)
{
	if (!root)
	{
		return nullptr;
	}

	MeshBuffer* buffer = Render_AllocMeshBuffer("octree");
	if (!buffer)
	{
		printf("Error! Could not allocate mesh buffer\n");
		return nullptr;
	}

	buffer->numTriangles = 0;
	buffer->numVertices = 0;

	GenerateVertexIndices(root, colour, buffer);
	ContourCellProc(root, buffer);

	if (buffer->numTriangles <= 0)
	{
		Render_FreeMeshBuffer(buffer);
		return nullptr;
	}
	/*
	else 
		printf("octree: %d triangles %d vertices\n",
			buffer->numTriangles, buffer->numVertices);
	*/

	return buffer;
}

// ----------------------------------------------------------------------------
