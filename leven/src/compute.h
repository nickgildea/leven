#ifndef		HAS_COMPUTE_H_BEEN_INCLUDED
#define		HAS_COMPUTE_H_BEEN_INCLUDED

#include	"render_types.h"
#include	"aabb.h"

#include	<vector>
#include	<glm/glm.hpp>
#include	<stdint.h>

// need an error value to return from compute functions that encounter an non-OpenCL error
#define LVN_CL_ERROR (-99999)

// ----------------------------------------------------------------------------

struct CSGOperationInfo
{
	int				type = 0;
	RenderShape		brushShape = RenderShape_Cube;
	int				material = 0;
	float			rotateY = 0.f;
	glm::vec4		origin;
	glm::vec4		dimensions;
};

struct SeamNodeInfo
{
	glm::ivec4		localspaceMin;
	glm::vec4		position;
	glm::vec4		normal;
};

// ----------------------------------------------------------------------------

int	Compute_Initialise(const int noiseSeed, const unsigned int defaultMaterial, const int numCSGBrushes);
int	Compute_Shutdown();

int Compute_SetNoiseSeed(const int noiseSeed);
int Compute_StoreCSGOperation(const CSGOperationInfo& opInfo, const AABB& aabb);
int Compute_ClearCSGOperations();

// ----------------------------------------------------------------------------

struct MeshGenerationContext;

class Compute_MeshGenContext
{
public:

	static Compute_MeshGenContext* create(const int voxelsPerChunk);

	int voxelsPerChunk() const;

	int applyCSGOperations(
		const std::vector<CSGOperationInfo>& opInfo,
		const glm::ivec3& clipmapNodeMin,
		const int clipmapNodeSize);

	int freeChunkOctree(
		const glm::ivec3& min,
		const int size);

	int isChunkEmpty(
		const glm::ivec3& min,
		const int size,
		bool& isEmpty);

	int generateChunkMesh(
		const glm::ivec3& min,
		const int clipmapNodeSize,
		MeshBuffer* meshBuffer,
		std::vector<SeamNodeInfo>& seamNodeBuffer);

private:

	MeshGenerationContext*     privateCtx_;
};

// ----------------------------------------------------------------------------

const char* GetCLErrorString(int error);

// ----------------------------------------------------------------------------

#endif	//	HAS_COMPUTE_H_BEEN_INCLUDED
