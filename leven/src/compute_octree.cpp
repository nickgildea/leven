#include	"compute_local.h"
#include	"compute_cuckoo.h"

#include	"volume_materials.h"
#include	"volume_constants.h"
#include	"timer.h"
#include	"file_utils.h"
#include	"glsl_svd.h"

#include	<vector>
#include	<sstream>
#include	<unordered_map>
#include	<glm/glm.hpp>
#include	<glm/gtx/integer.hpp>
using namespace glm;
#include	<Remotery.h>

// ----------------------------------------------------------------------------

// TODO remove
const glm::vec3 ColourForMinLeafSize(const int minLeafSize);

// ----------------------------------------------------------------------------

int ConstructOctreeFromField(
	MeshGenerationContext* meshGen,
	const glm::ivec3& min,
	const GPUDensityField& field,
	GPUOctree* octree)
{
	rmt_ScopedCPUSample(ConstructOctree);
//	printf("Constuct octree (%d %d %d)\n", min.x, min.y, min.z);
	Timer timer;
	timer.start();
	timer.disable();

	timer.printElapsed("initialise field");

	if (field.numEdges == 0)
	{
		// no voxels to find 
		timer.printElapsed("no edges");
		return CL_SUCCESS;
	}

	auto ctx = GetComputeContext();

	const int chunkBufferSize = meshGen->voxelsPerChunk * meshGen->voxelsPerChunk * meshGen->voxelsPerChunk;
	cl::Buffer d_leafOccupancy(ctx->context, CL_MEM_READ_WRITE, chunkBufferSize * sizeof(int));
	cl::Buffer d_leafEdgeInfo(ctx->context, CL_MEM_READ_WRITE, chunkBufferSize * sizeof(int));
	cl::Buffer d_leafCodes(ctx->context, CL_MEM_READ_WRITE, chunkBufferSize * sizeof(int));
	cl::Buffer d_leafMaterials(ctx->context, CL_MEM_READ_WRITE, chunkBufferSize * sizeof(cl_int));
	cl::Buffer d_voxelScan(ctx->context, CL_MEM_READ_WRITE, chunkBufferSize * sizeof(int));
	{
		rmt_ScopedCPUSample(Find);

		int index = 0;
		cl::Kernel findActiveKernel(meshGen->octreeProgram.get(), "FindActiveVoxels");
		CL_CALL(findActiveKernel.setArg(index++, field.materials));
		CL_CALL(findActiveKernel.setArg(index++, d_leafOccupancy));
		CL_CALL(findActiveKernel.setArg(index++, d_leafEdgeInfo));
		CL_CALL(findActiveKernel.setArg(index++, d_leafCodes));
		CL_CALL(findActiveKernel.setArg(index++, d_leafMaterials));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(findActiveKernel, cl::NullRange, 
			cl::NDRange(meshGen->voxelsPerChunk, meshGen->voxelsPerChunk, meshGen->voxelsPerChunk), cl::NullRange));

		octree->numNodes = ExclusiveScan(ctx->queue, d_leafOccupancy, d_voxelScan, chunkBufferSize);
		if (octree->numNodes <= 0)
		{
			// i.e. an error if < 0, == 0 is ok just no surface for this chunk
			timer.printElapsed("no voxels");
			const int error = octree->numNodes;
			octree->numNodes = 0;
			return error;
		}
	}

	cl::Buffer d_compactLeafEdgeInfo(ctx->context, CL_MEM_READ_WRITE, octree->numNodes * sizeof(int));
	{
		rmt_ScopedCPUSample(Compact);

		CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * octree->numNodes, nullptr, octree->d_nodeCodes));
		CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * octree->numNodes, nullptr, octree->d_nodeMaterials));

		CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_float4) * octree->numNodes, nullptr, octree->d_vertexPositions));
		CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_float4) * octree->numNodes, nullptr, octree->d_vertexNormals));

		int index = 0;
		cl::Kernel compactVoxelsKernel(meshGen->octreeProgram.get(), "CompactVoxels");
		CL_CALL(compactVoxelsKernel.setArg(index++, d_leafOccupancy));
		CL_CALL(compactVoxelsKernel.setArg(index++, d_leafEdgeInfo));
		CL_CALL(compactVoxelsKernel.setArg(index++, d_leafCodes));
		CL_CALL(compactVoxelsKernel.setArg(index++, d_leafMaterials));
		CL_CALL(compactVoxelsKernel.setArg(index++, d_voxelScan));
		CL_CALL(compactVoxelsKernel.setArg(index++, octree->d_nodeCodes));
		CL_CALL(compactVoxelsKernel.setArg(index++, d_compactLeafEdgeInfo));
		CL_CALL(compactVoxelsKernel.setArg(index++, octree->d_nodeMaterials));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(compactVoxelsKernel, cl::NullRange, chunkBufferSize, cl::NullRange));
	}

	cl::Buffer d_qefs;
	{
		rmt_ScopedCPUSample(Leafs);

		CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(QEFData) * octree->numNodes, nullptr, d_qefs));

		CuckooData edgeHashTable;
		CL_CALL(Cuckoo_InitialiseTable(&edgeHashTable, field.numEdges));
		CL_CALL(Cuckoo_InsertKeys(&edgeHashTable, field.edgeIndices, field.numEdges));

		int index = 0;
		const int sampleScale = field.size / (meshGen->voxelsPerChunk * LEAF_SIZE_SCALE);
		cl::Kernel createLeafNodes(meshGen->octreeProgram.get(), "CreateLeafNodes");
		CL_CALL(createLeafNodes.setArg(index++, sampleScale));
		CL_CALL(createLeafNodes.setArg(index++, octree->d_nodeCodes));
		CL_CALL(createLeafNodes.setArg(index++, d_compactLeafEdgeInfo));
		CL_CALL(createLeafNodes.setArg(index++, field.normals));
		CL_CALL(createLeafNodes.setArg(index++, octree->d_vertexNormals));
		CL_CALL(createLeafNodes.setArg(index++, d_qefs));
		CL_CALL(createLeafNodes.setArg(index++, edgeHashTable.table));
		CL_CALL(createLeafNodes.setArg(index++, edgeHashTable.stash));
		CL_CALL(createLeafNodes.setArg(index++, edgeHashTable.prime));
		CL_CALL(createLeafNodes.setArg(index++, edgeHashTable.hashParams));
		CL_CALL(createLeafNodes.setArg(index++, edgeHashTable.stashUsed));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(createLeafNodes, cl::NullRange, octree->numNodes, cl::NullRange));
	}

	{
		rmt_ScopedCPUSample(QEF);

		cl_float4 d_worldSpaceOffset = { min.x, min.y, min.z, 0 };
		cl::Kernel solveQEFs(meshGen->octreeProgram.get(), "SolveQEFs");
		int index = 0;
		CL_CALL(solveQEFs.setArg(index++, d_worldSpaceOffset));
		CL_CALL(solveQEFs.setArg(index++, d_qefs));
		CL_CALL(solveQEFs.setArg(index++, octree->d_vertexPositions));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(solveQEFs, cl::NullRange, octree->numNodes, cl::NullRange));
	}

	{
		rmt_ScopedCPUSample(Cuckoo);

		CL_CALL(Cuckoo_InitialiseTable(&octree->d_hashTable, octree->numNodes));
		CL_CALL(Cuckoo_InsertKeys(&octree->d_hashTable, octree->d_nodeCodes, octree->numNodes));
	}


	timer.printElapsed("done");
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int LoadOctree(MeshGenerationContext* meshGen, const ivec3& min, const int clipmapNodeSize, GPUOctree* octree)
{
	rmt_ScopedCPUSample(LoadOctree);
	const ivec4 key(min, clipmapNodeSize);
	auto iter = meshGen->octreeCache.find(key);
	if (iter != end(meshGen->octreeCache))
	{
		*octree = iter->second;
		return CL_SUCCESS;
	}

	GPUDensityField field;
	CL_CALL(LoadDensityField(meshGen, min, clipmapNodeSize, &field));
	if (field.numEdges == 0)
	{
		// no point in trying to construct the octree
		octree->numNodes = 0;
	}
	else	
	{
		CL_CALL(ConstructOctreeFromField(meshGen, min, field, octree));
		meshGen->octreeCache[key] = *octree;
	}

//	printf("%d octrees cached\n", meshGen->octreeCache.size());

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------
// Use the nodes extracted from the octree(s) to generate a mesh. The nodes in 
// the buffer are treated as leaf nodes in an octree and as such their positions
// should be in the [0, VOXELS_PER_CHUNK) range -- when constructing the buffer from
// multiple octrees the nodes will need to be remapped to the new composite octree
// ----------------------------------------------------------------------------
int GenerateMeshFromOctree(
	MeshGenerationContext* meshGen,
	const glm::ivec3& min,
	const int clipmapNodeSize,
	const GPUOctree& octree,
	MeshBufferGPU* meshBuffer)
{
	rmt_ScopedCPUSample(GenerateMeshFromOctree);
	ComputeContext* ctx = GetComputeContext();
	int index = 0;

//	printf("Generate mesh: size=%d\n", clipmapNodeSize);
	Timer timer;
	timer.start();
	timer.disable();

	const int numVertices = octree.numNodes;
	const int indexBufferSize = numVertices * 6 * 3;
	const int trianglesValidSize = numVertices * 3;
	cl::Buffer d_indexBuffer, d_trianglesValid;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * indexBufferSize, nullptr, d_indexBuffer));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * trianglesValidSize, nullptr, d_trianglesValid));

	index = 0;
	cl::Kernel k_GenerateMesh(meshGen->octreeProgram.get(), "GenerateMesh");
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_nodeCodes));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_nodeMaterials));
	CL_CALL(k_GenerateMesh.setArg(index++, d_indexBuffer));
	CL_CALL(k_GenerateMesh.setArg(index++, d_trianglesValid));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_hashTable.table));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_hashTable.stash));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_hashTable.prime));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_hashTable.hashParams));
	CL_CALL(k_GenerateMesh.setArg(index++, octree.d_hashTable.stashUsed));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_GenerateMesh, cl::NullRange, octree.numNodes, cl::NullRange));

	cl::Buffer d_trianglesScan;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * trianglesValidSize, nullptr, d_trianglesScan));
	int numTriangles = ExclusiveScan(ctx->queue, d_trianglesValid, d_trianglesScan, trianglesValidSize); 
	if (numTriangles <= 0)
	{
		// < 0 is an error, so return that, 0 is ok just no tris to generate, return 0 which is CL_SUCCESS
		return numTriangles;
	}

	numTriangles *= 2;
	LVN_ALWAYS_ASSERT("Mesh triangle count too high", numTriangles < MAX_MESH_TRIANGLES);
	LVN_ALWAYS_ASSERT("Mesh vertex count too high", numVertices < MAX_MESH_VERTICES);

	cl::Buffer d_compactIndexBuffer;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * numTriangles * 3, nullptr, d_compactIndexBuffer));

	index = 0;
	cl::Kernel k_CompactMeshTriangles(meshGen->octreeProgram.get(), "CompactMeshTriangles");
	CL_CALL(k_CompactMeshTriangles.setArg(index++, d_trianglesValid));
	CL_CALL(k_CompactMeshTriangles.setArg(index++, d_trianglesScan));
	CL_CALL(k_CompactMeshTriangles.setArg(index++, d_indexBuffer));
	CL_CALL(k_CompactMeshTriangles.setArg(index++, d_compactIndexBuffer));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_CompactMeshTriangles, cl::NullRange, trianglesValidSize, cl::NullRange));

	cl::Buffer d_vertexBuffer;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(MeshVertex) * numVertices, nullptr, d_vertexBuffer));

	index = 0;
	const auto colour = ColourForMinLeafSize(clipmapNodeSize / CLIPMAP_LEAF_SIZE);
	cl_float4 d_colour = { colour.x, colour.y, colour.z, 0.f };
	cl::Kernel k_GenerateMeshVertexBuffer(meshGen->octreeProgram.get(), "GenerateMeshVertexBuffer");
	CL_CALL(k_GenerateMeshVertexBuffer.setArg(index++, octree.d_vertexPositions));
	CL_CALL(k_GenerateMeshVertexBuffer.setArg(index++, octree.d_vertexNormals));
	CL_CALL(k_GenerateMeshVertexBuffer.setArg(index++, octree.d_nodeMaterials));
	CL_CALL(k_GenerateMeshVertexBuffer.setArg(index++, d_colour));
	CL_CALL(k_GenerateMeshVertexBuffer.setArg(index++, d_vertexBuffer));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_GenerateMeshVertexBuffer, cl::NullRange, numVertices, cl::NullRange));

	meshBuffer->vertices = d_vertexBuffer;
	meshBuffer->countVertices = numVertices;
	meshBuffer->triangles = d_compactIndexBuffer;
	meshBuffer->countTriangles = numTriangles;

	timer.printElapsed("generated mesh");

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int GatherSeamNodesFromOctree(
	MeshGenerationContext* meshGen,
	const glm::ivec3& nodeMin, 
	const int nodeSize,
	const GPUOctree& octree,
	std::vector<SeamNodeInfo>& seamNodeBuffer)
{
	rmt_ScopedCPUSample(GatherSeamNodes);
	auto ctx = GetComputeContext();

	cl::Buffer d_isSeamNode, d_isSeamNodeScan;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * octree.numNodes, nullptr, d_isSeamNode));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(cl_int) * octree.numNodes, nullptr, d_isSeamNodeScan));

	int index = 0;
	cl::Kernel k_FindSeamNodes(meshGen->octreeProgram.get(), "FindSeamNodes");
	CL_CALL(k_FindSeamNodes.setArg(index++, octree.d_nodeCodes));
	CL_CALL(k_FindSeamNodes.setArg(index++, d_isSeamNode));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_FindSeamNodes, cl::NullRange, octree.numNodes, cl::NullRange));

	const int numSeamNodes = ExclusiveScan(ctx->queue, d_isSeamNode, d_isSeamNodeScan, octree.numNodes);
	if (numSeamNodes <= 0)
	{
		return numSeamNodes;
	}

	cl::Buffer d_seamNodeInfo;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(SeamNodeInfo) * numSeamNodes, nullptr, d_seamNodeInfo));

	cl_int4 d_min = { nodeMin.x, nodeMin.y, nodeMin.z, 0 };

	index = 0;
	cl::Kernel k_ExtractSeamNodeInfo(meshGen->octreeProgram.get(), "ExtractSeamNodeInfo");
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, d_isSeamNode));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, d_isSeamNodeScan));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, octree.d_nodeCodes));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, octree.d_nodeMaterials));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, octree.d_vertexPositions));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, octree.d_vertexNormals));
	CL_CALL(k_ExtractSeamNodeInfo.setArg(index++, d_seamNodeInfo));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_ExtractSeamNodeInfo, cl::NullRange, octree.numNodes, cl::NullRange));

	seamNodeBuffer.resize(numSeamNodes);
	CL_CALL(ctx->queue.enqueueReadBuffer(d_seamNodeInfo, CL_TRUE, 
		0, sizeof(SeamNodeInfo) * numSeamNodes, &seamNodeBuffer[0]));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int ExportMeshBuffer(
	const MeshBufferGPU& gpuBuffer,
	MeshBuffer* cpuBuffer)
{
	rmt_ScopedCPUSample(ExportMesh);
	cpuBuffer->numVertices = gpuBuffer.countVertices;
	cpuBuffer->numTriangles = gpuBuffer.countTriangles;

	if (gpuBuffer.countVertices == 0)
	{
		return CL_SUCCESS;
	}
	
	auto ctx = GetComputeContext();

	CL_CALL(ctx->queue.enqueueReadBuffer(gpuBuffer.vertices, CL_FALSE, 
		0, sizeof(MeshVertex) * gpuBuffer.countVertices, &cpuBuffer->vertices[0]));
	CL_CALL(ctx->queue.enqueueReadBuffer(gpuBuffer.triangles, CL_TRUE, 
		0, sizeof(MeshTriangle) * gpuBuffer.countTriangles, &cpuBuffer->triangles[0]));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_GenerateChunkMesh(
	MeshGenerationContext* meshGen,
	const glm::ivec3& min,
	const int clipmapNodeSize,
	MeshBuffer* meshBuffer,
	std::vector<SeamNodeInfo>& seamNodeBuffer)
{
	rmt_ScopedCPUSample(Compute_GenerateChunkMesh);
	seamNodeBuffer.clear();

	GPUOctree octree;
	CL_CALL(LoadOctree(meshGen, min, clipmapNodeSize, &octree));

	if (octree.numNodes > 0)
	{
		MeshBufferGPU meshBufferGPU;
		CL_CALL(GenerateMeshFromOctree(meshGen, min, clipmapNodeSize, octree, &meshBufferGPU));
		CL_CALL(ExportMeshBuffer(meshBufferGPU, meshBuffer));

		// TODO can do this on creation now 
		CL_CALL(GatherSeamNodesFromOctree(meshGen, min, clipmapNodeSize, octree, seamNodeBuffer));
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_FreeChunkOctree(
	MeshGenerationContext* meshGen, 
	const glm::ivec3& min, 
	const int clipmapNodeSize)
{
	const glm::ivec4 key(min, clipmapNodeSize);
	meshGen->octreeCache.erase(key);
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------
