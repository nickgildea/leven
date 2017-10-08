#ifndef		HAS_COMPUTE_LOCAL_H_BEEN_INCLUDED
#define		HAS_COMPUTE_LOCAL_H_BEEN_INCLUDED

#include	"compute.h"
#include	"compute_program.h"
#include	"compute_cuckoo.h"
#include	"glm_hash.h"

#include	<CL/cl.hpp>
#include	<string>
#include	<stdint.h>
#include	<glm/glm.hpp>
#include	<unordered_map>

// ----------------------------------------------------------------------------

struct ComputeContext
{
	cl::Device          device;
	cl::Context         context;
	cl::CommandQueue    queue;
	cl::Image2D         noisePermLookupImage;
	int                 defaultMaterial = 0;
};

// ----------------------------------------------------------------------------

struct GPUDensityField
{
	glm::ivec3          min;
	int                 size = 0;
	int                 lastCSGOperation = 0;

	unsigned int        numEdges = 0;
	cl::Buffer          edgeIndices;
	cl::Buffer          normals;
	cl::Buffer          materials;
};

typedef std::unordered_map<glm::ivec4, GPUDensityField> DensityFieldCache;

// ----------------------------------------------------------------------------

struct GPUOctree
{
	int             numNodes = 0;
	cl::Buffer      d_nodeCodes, d_nodeMaterials;
	cl::Buffer      d_vertexPositions, d_vertexNormals;
	CuckooData      d_hashTable;
};

// TODO a better cache than this!
typedef std::unordered_map<glm::ivec4, GPUOctree> OctreeCache;

// ----------------------------------------------------------------------------

struct MeshGenerationContext
{
	ComputeProgram      densityFieldProgram;
	DensityFieldCache   densityFieldCache;

	ComputeProgram      octreeProgram;
	OctreeCache         octreeCache;

	ComputeProgram      csgProgram;

	int                 voxelsPerChunk = -1;
	int                 hermiteIndexSize = -1;
	int                 fieldSize = -1;
	int                 indexShift = -1;
	int                 indexMask = -1;
};

// ----------------------------------------------------------------------------

struct MeshBufferGPU
{
	MeshBufferGPU()
		: countVertices(0)
		, countTriangles(0)
	{
	}

	cl::Buffer            vertices, triangles;
    int	                countVertices, countTriangles;
};

// ----------------------------------------------------------------------------
// previously the external API (now wrapped in Compute_MeshGenContext class) 
// TODO: remove?

Compute_MeshGenContext* Compute_CreateMeshGenerator(const int voxelsPerChunk);

int Compute_ApplyCSGOperations(
	MeshGenerationContext* meshGen,
	const std::vector<CSGOperationInfo>& opInfo,
	const glm::ivec3& clipmapNodeMin,
	const int clipmapNodeSize);

int Compute_FreeChunkOctree(
	MeshGenerationContext* meshGen, 
	const glm::ivec3& min, 
	const int clipmapNodeSize);

int Compute_ChunkIsEmpty(
	MeshGenerationContext* meshGen, 
	const glm::ivec3& min, 
	const int chunkSize, 
	bool& isEmpty);

int Compute_GenerateChunkMesh(
	MeshGenerationContext* meshGen,
	const glm::ivec3& min,
	const int clipmapNodeSize,
	MeshBuffer* meshBuffer,
	std::vector<SeamNodeInfo>& seamNodeBuffer);

// ----------------------------------------------------------------------------

ComputeContext* GetComputeContext();

int CreateBuffer(
	cl_int permissions,
	u32 bufferSize,
	void* hostDataPtr,
	cl::Buffer& buffer);

int Scan(cl::CommandQueue& queue, cl::Buffer& data, cl::Buffer& scanData, const int count, const bool exclusive);
int ExclusiveScan(cl::CommandQueue& queue, cl::Buffer& data, cl::Buffer& scan, const u32 count);

int CompactArray_Long(
	cl::CommandQueue&	q,
	cl::Buffer&			valuesArray,
	cl::Buffer&			validity,
	const u32		count,
	cl::Buffer&			compactArray);

int CompactIndexArray(cl::CommandQueue& queue, cl::Buffer& indexArray, 
					  cl::Buffer& validity, const int count, cl::Buffer& compactArray);

cl::Buffer RemoveDuplicates(
	cl::CommandQueue& queue, 
	cl::Buffer& inputData, 
	const int inputCount,
	unsigned int* resultCount);

int FillBufferInt(
	cl::CommandQueue& queue, 
	cl::Buffer& buffer, 
	const u32 count, 
	const cl_int value);

int FillBufferLong(
	cl::CommandQueue& queue, 
	cl::Buffer& buffer, 
	const u32 count, 
	const cl_long value);

// ----------------------------------------------------------------------------

int CalculateAmbientOcclusion(
	const int clipmapNodeSize,
	const cl::Buffer& d_vertexPositions,
	const cl::Buffer& d_vertexNormals,
	const int numVertices);

int ApplyCSGOperations(
	MeshGenerationContext* meshGen,
	const std::vector<CSGOperationInfo>& opInfo,
	const glm::ivec3& clipmapNodeMin,
	const int clipmapNodeSize,
	GPUDensityField& field);

int LoadDensityField(
	MeshGenerationContext* meshGen, 
	const glm::ivec3& min, 
	const int clipmapNodeSize, 
	GPUDensityField* field);

int StoreDensityField(
	MeshGenerationContext* meshGen, 
	const GPUDensityField& field);

// ----------------------------------------------------------------------------

cl::size_t<3> Size3(const u32 size);
cl::size_t<3> Size3(const u32 x, const u32 y, const u32 z);
cl_int4 LeafScaleVec(const glm::ivec3& v);
cl_float4 LeafScaleVec(const glm::vec3& v);
cl_float4 LeafScaleVec(const glm::vec4& v);

// ----------------------------------------------------------------------------

#define CL_CALL(fn)														\
{																		\
	cl_int __cl_call_error = (fn);										\
	if (__cl_call_error != CL_SUCCESS)									\
	{																	\
		printf("OpenCL error=%s calling '" #fn "'\n", GetCLErrorString(__cl_call_error));	\
		printf("  file: %s line: %d\n", __FILE__, __LINE__);			\
		__debugbreak(); \
		return __cl_call_error;											\
	}																	\
}

#define CL_CHECK_ERROR(error, msg)										\
{																		\
	if ((error) != CL_SUCCESS)											\
	{																	\
		printf("OpenCL error=%s for '%s'\n", GetCLErrorString(error), (msg));			\
		printf("  file: %s line: %d\n", __FILE__, __LINE__);			\
		return (error);													\
	}																	\
}

#endif	//	HAS_COMPUTE_LOCAL_H_BEEN_INCLUDED
