#include	"compute_local.h"
#include	"compute_program.h"
#include	"volume_constants.h"
#include	"volume_materials.h"
#include	"timer.h"

#include	<sstream>
#include	<random>
#include	<algorithm>
#include	<array>
#include	<unordered_map>
#include	<Remotery.h>

using glm::ivec2; 
using glm::ivec3;
using glm::ivec4;
using glm::vec3;
using glm::vec4;

// ----------------------------------------------------------------------------

// store the AABBs seperately so the ops can be written to the CL buffers without processing
std::vector<AABB> g_storedOpAABBs;		
std::vector<CSGOperationInfo> g_storedOps;

// ----------------------------------------------------------------------------

int perm[512]= {151,160,137,91,90,15,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180};

/* These are Ken Perlin's proposed gradients for 3D noise. I kept them for
   better consistency with the reference implementation, but there is really
   no need to pad this to 16 gradients for this particular implementation.
   If only the "proper" first 12 gradients are used, they can be extracted
   from the grad4[][] array: grad3[i][j] == grad4[i*2][j], 0<=i<=11, j=0,1,2
*/
int grad3[16][3] = {{0,1,1},{0,1,-1},{0,-1,1},{0,-1,-1},
                   {1,0,1},{1,0,-1},{-1,0,1},{-1,0,-1},
                   {1,1,0},{1,-1,0},{-1,1,0},{-1,-1,0}, // 12 cube edges
                   {1,0,-1},{-1,0,-1},{0,-1,1},{0,1,1}}; // 4 more to make 16



// based on jenkins hash
unsigned int NoiseHash(const int x, const int y, const int seed)
{
	const int key = ((x << 24) | (y << 16)) ^ seed;
	unsigned char* keyBytes = (unsigned char*)&key;

	unsigned int hash = 0;
	for (int i = 0; i < 4; i++)
	{
		hash += keyBytes[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

// ----------------------------------------------------------------------------

int CreateNoisePermutationLookupImage(const int seed)
{
	std::array<int, 512> shuffledPerm;
	for (int i = 0; i < 512; i++)
	{
		shuffledPerm[i] = perm[i];
	}

	std::shuffle(begin(shuffledPerm), end(shuffledPerm), std::default_random_engine(seed));

	std::vector<unsigned char> pixels(256 * 256 * 4);
	for (int i = 0; i < 256; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			const int offset = ((i * 256) + j) * 4;
			const unsigned char value = shuffledPerm[NoiseHash(i, j, seed) & 0x1ff];

			pixels[offset + 0] = grad3[value & 0x0f][0] * 64 + 64;
			pixels[offset + 1] = grad3[value & 0x0f][1] * 64 + 64;
			pixels[offset + 2] = grad3[value & 0x0f][2] * 64 + 64;
			pixels[offset + 3] = value;
		}
	}

	cl::ImageFormat format(CL_RGBA, CL_UNORM_INT8);

	auto ctx = GetComputeContext();
	ctx->noisePermLookupImage = cl::Image2D(ctx->context, CL_MEM_READ_ONLY, format, 256, 256);

	const cl::size_t<3> origin = Size3(0);
	const cl::size_t<3> region = Size3(256, 256, 1);

	CL_CALL(ctx->queue.enqueueWriteImage(ctx->noisePermLookupImage, CL_TRUE, origin, region, 0, 0, &pixels[0]));
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_SetNoiseSeed(const int noiseSeed)
{
	CL_CALL(CreateNoisePermutationLookupImage(noiseSeed));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int GenerateDefaultDensityField(MeshGenerationContext* meshGen, GPUDensityField* field)
{
	rmt_ScopedCPUSample(GenerateField);

	auto ctx = GetComputeContext();

	const int fieldBufferSize = meshGen->fieldSize * meshGen->fieldSize * meshGen->fieldSize; 
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, fieldBufferSize * sizeof(cl_int), nullptr, field->materials));
	const cl_int4 d_fieldOffset = LeafScaleVec(field->min);

	int index = 0;
	const int sampleScale = field->size / (meshGen->voxelsPerChunk * LEAF_SIZE_SCALE);
	cl::Kernel generateFieldKernel(meshGen->densityFieldProgram.get(), "GenerateDefaultField");
	CL_CALL(generateFieldKernel.setArg(index++, ctx->noisePermLookupImage));
	CL_CALL(generateFieldKernel.setArg(index++, d_fieldOffset));
	CL_CALL(generateFieldKernel.setArg(index++, sampleScale));
	CL_CALL(generateFieldKernel.setArg(index++, ctx->defaultMaterial));
	CL_CALL(generateFieldKernel.setArg(index++, field->materials));

	cl::NDRange generateFieldSize(meshGen->fieldSize, meshGen->fieldSize, meshGen->fieldSize);
	CL_CALL(ctx->queue.enqueueNDRangeKernel(generateFieldKernel, cl::NullRange, generateFieldSize, cl::NullRange));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int FindDefaultEdges(MeshGenerationContext* meshGen, GPUDensityField* field)
{
	rmt_ScopedCPUSample(FindDefaultEdges);

	cl_int4 fieldOffset;
	fieldOffset.s[0] = field->min.x / LEAF_SIZE_SCALE;
	fieldOffset.s[1] = field->min.y / LEAF_SIZE_SCALE;
	fieldOffset.s[2] = field->min.z / LEAF_SIZE_SCALE;
	fieldOffset.s[3] = 0;

	auto ctx = GetComputeContext();

	cl::Buffer edgeOccupancy;
	const int edgeBufferSize = meshGen->hermiteIndexSize * meshGen->hermiteIndexSize * meshGen->hermiteIndexSize * 3;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, edgeBufferSize * sizeof(cl_int), nullptr, edgeOccupancy));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, edgeBufferSize * sizeof(cl_int), nullptr, field->edgeIndices));

	int index = 0;
	cl::Kernel k_findEdges(meshGen->densityFieldProgram.get(), "FindFieldEdges");
	CL_CALL(k_findEdges.setArg(index++, fieldOffset));
	CL_CALL(k_findEdges.setArg(index++, field->materials));
	CL_CALL(k_findEdges.setArg(index++, edgeOccupancy));
	CL_CALL(k_findEdges.setArg(index++, field->edgeIndices));

	cl::NDRange globalSize(meshGen->hermiteIndexSize, meshGen->hermiteIndexSize, meshGen->hermiteIndexSize);
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_findEdges, cl::NullRange, globalSize, cl::NullRange));

	cl::Buffer edgeScan(ctx->context, CL_MEM_READ_WRITE, edgeBufferSize * sizeof(int));
	field->numEdges = ExclusiveScan(ctx->queue, edgeOccupancy, edgeScan, edgeBufferSize);
	if (field->numEdges < 0)
	{
		printf("FindDefaultEdges: ExclusiveScan error=%d\n", field->numEdges);
		return field->numEdges;
	}

	if (field->numEdges == 0)
	{
		// nothing to do here
		return CL_SUCCESS;
	}

	cl::Buffer compactActiveEdges(ctx->context, CL_MEM_READ_WRITE, field->numEdges * sizeof(int));

	index = 0;
	cl::Kernel k_compactEdges(meshGen->densityFieldProgram.get(), "CompactEdges");
	CL_CALL(k_compactEdges.setArg(index++, edgeOccupancy));
	CL_CALL(k_compactEdges.setArg(index++, edgeScan));
	CL_CALL(k_compactEdges.setArg(index++, field->edgeIndices));
	CL_CALL(k_compactEdges.setArg(index++, compactActiveEdges));

	const size_t compactEdgesSize = edgeBufferSize;
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_compactEdges, cl::NullRange, compactEdgesSize, cl::NullRange));

	field->edgeIndices = compactActiveEdges;
	field->normals = cl::Buffer(ctx->context, CL_MEM_WRITE_ONLY, sizeof(glm::vec4) * field->numEdges);

	index = 0;
	const int sampleScale = field->size / (meshGen->voxelsPerChunk * LEAF_SIZE_SCALE);
	cl::Kernel k_findInfo(meshGen->densityFieldProgram.get(), "FindEdgeIntersectionInfo");
	CL_CALL(k_findInfo.setArg(index++, ctx->noisePermLookupImage));
	CL_CALL(k_findInfo.setArg(index++, fieldOffset));
	CL_CALL(k_findInfo.setArg(index++, sampleScale));
	CL_CALL(k_findInfo.setArg(index++, field->edgeIndices));
	CL_CALL(k_findInfo.setArg(index++, field->normals));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(k_findInfo, cl::NullRange, field->numEdges, cl::NullRange));
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int LoadDensityField(MeshGenerationContext* meshGen, const glm::ivec3& min, const int clipmapNodeSize, GPUDensityField* field)
{
	rmt_ScopedCPUSample(LoadDensityField);

	const glm::ivec4 key(min, clipmapNodeSize);
	const auto iter = meshGen->densityFieldCache.find(key);
	if (iter != end(meshGen->densityFieldCache))
	{
		*field = iter->second;
		LVN_ASSERT(field->min == min);
	}
	else
	{
		field->min = min;
		field->size = clipmapNodeSize;

		CL_CALL(GenerateDefaultDensityField(meshGen, field)); 
		CL_CALL(FindDefaultEdges(meshGen, field));
	}

	const AABB fieldBB(field->min, field->size);
	std::vector<CSGOperationInfo> csgOperations;
	for (int i = field->lastCSGOperation; i < g_storedOps.size(); i++)
	{
		if (fieldBB.overlaps(g_storedOpAABBs[i]))
		{
			csgOperations.push_back(g_storedOps[i]);
		}
	}

	field->lastCSGOperation = g_storedOps.size();

	if (!csgOperations.empty())
	{
		CL_CALL(ApplyCSGOperations(meshGen, csgOperations, field->min, field->size, *field));
		CL_CALL(StoreDensityField(meshGen, *field));
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_ChunkIsEmpty(MeshGenerationContext* meshGen, const glm::ivec3& min, const int chunkSize, bool& isEmpty)
{
	const ivec4 key = ivec4(min, chunkSize);
	const auto iter = meshGen->densityFieldCache.find(key);
	if (iter != end(meshGen->densityFieldCache))
	{
		const auto& field = iter->second;
		isEmpty = field.numEdges > 0;
		return CL_SUCCESS;
	}

	GPUDensityField field;
	field.min = min;
	field.size = chunkSize;

	CL_CALL(GenerateDefaultDensityField(meshGen, &field));
	CL_CALL(FindDefaultEdges(meshGen, &field));
	CL_CALL(StoreDensityField(meshGen, field));
	isEmpty = field.numEdges > 0;

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int StoreDensityField(MeshGenerationContext* meshGen, const GPUDensityField& field)
{
	const glm::ivec4 key(field.min, field.size);
	meshGen->densityFieldCache[key] = field;
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_StoreCSGOperation(const CSGOperationInfo& opInfo, const AABB& aabb)
{
	g_storedOps.push_back(opInfo);
	g_storedOpAABBs.push_back(aabb);
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_ClearCSGOperations()
{
	g_storedOps.clear();
	g_storedOpAABBs.clear();
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

