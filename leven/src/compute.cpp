#include	"compute.h"

#include	"compute_local.h"
#include	"compute_cuckoo.h"
#include	"compute_program.h"

#include	"volume_constants.h"
#include	"volume_materials.h"
#include	"timer.h"
#include	"file_utils.h"
#include	"glsl_svd.h"
#include	"volume.h"		
#include	"primes.h"
#include	"lrucache.h"

#include	<Remotery.h>
#include	"sdl_wrapper.h"
#include	<functional>
#include	<sstream>
#include	<unordered_map>
#include	<array>
#include	<random>
#include	<memory>
#include	<glm/gtx/integer.hpp>

// ----------------------------------------------------------------------------

ComputeProgram g_utilProgram;

// ----------------------------------------------------------------------------

int CreateBuffer(
	cl_int permissions,
	u32 bufferSize,
	void* hostDataPtr,
	cl::Buffer& buffer)
{
	cl_int error = CL_SUCCESS;
	auto ctx = GetComputeContext();
	if (hostDataPtr)
	{
		permissions |= CL_MEM_COPY_HOST_PTR;
	}

	buffer = cl::Buffer(ctx->context, permissions, bufferSize, hostDataPtr, &error);
	if (buffer() == 0)
	{
		printf("clCreateBuffer returned null pointer: size=%d error=%d\n", bufferSize, error);
	}

	return error;
}

// ----------------------------------------------------------------------------

int FillBufferInt(
	cl::CommandQueue& queue, 
	cl::Buffer& buffer, 
	const u32 count, 
	const cl_int value)
{
	cl::Kernel k_FillBuffer(g_utilProgram.get(), "FillBufferInt");
	int index = 0;
	CL_CALL(k_FillBuffer.setArg(index++, buffer));
	CL_CALL(k_FillBuffer.setArg(index++, value));
	CL_CALL(queue.enqueueNDRangeKernel(k_FillBuffer, 0, count));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int FillBufferLong(
	cl::CommandQueue& queue, 
	cl::Buffer& buffer, 
	const u32 count, 
	const cl_long value)
{
	cl::Kernel k_FillBuffer(g_utilProgram.get(), "FillBufferLong");
	int index = 0;
	CL_CALL(k_FillBuffer.setArg(index++, buffer));
	CL_CALL(k_FillBuffer.setArg(index++, value));
	CL_CALL(queue.enqueueNDRangeKernel(k_FillBuffer, 0, count));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int InitialiseContext(ComputeContext* ctx)
{
	if (ctx->context())
	{
		// already initialised
		return CL_SUCCESS;
	}

	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	if (platforms.empty())
	{
		printf("OpenCL: no platforms\n");
		return LVN_CL_ERROR;
	}


#ifdef USE_OPENGL_INTEROP
	cl_context_properties properties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0
	};

	clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = 
		(clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddressForPlatform(platforms[0](), "clGetGLContextInfoKHR");

	cl_device_id device_ids[32]; 
	size_t size = 0;
	CL_CALL(clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 
		sizeof(cl_device_id) * 32, &device_ids[0], &size));
	const u32 countDevices = size / sizeof(cl_device_id);
	printf("Found %d devices capable of OpenGL/OpenCL interop\n", countDevices);

	std::vector<cl::Device> devices;
	for (u32 i = 0; i < countDevices; i++)
	{
		devices.push_back(cl::Device(device_ids[i]));
	}

	ctx->context = cl::Context(devices, properties);
	ctx->device = devices[0];
#else
	cl_context_properties properties[] =
	{
		CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0
	};

	ctx->context = cl::Context(CL_DEVICE_TYPE_GPU, properties);
	auto devices = ctx->context.getInfo<CL_CONTEXT_DEVICES>();
	printf("OpenCL found %d devices\n", devices.size());
	if (devices.empty())
	{
		return LVN_CL_ERROR;
	}

	ctx->device = devices[0];
#endif

	cl_int err = 0;
	ctx->queue = cl::CommandQueue(ctx->context, ctx->device, 0, &err);
	if (err < 0)
	{
		printf("Couldn't create queue\n");
		return err;
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

//#define CONTEXT_PER_THREAD
#ifdef CONTEXT_PER_THREAD
__declspec(thread) ComputeContext* context;
#endif

ComputeContext* GetComputeContext()
{
	static std::unique_ptr<ComputeContext> computeContext = nullptr;

	if (computeContext == nullptr)
	{
		computeContext.reset(new ComputeContext);
		InitialiseContext(computeContext.get());
	}

#ifdef CONTEXT_PER_THREAD
	if (!context)
	{
		context = new ComputeContext;
		context->context = computeContext->context;
		context->device = computeContext->device;
		context->queue = cl::CommandQueue(computeContext->context, computeContext->device, 0);
	}

	return context;
#else
	return computeContext.get();
#endif
}

// ----------------------------------------------------------------------------

int Compute_Initialise(const int noiseSeed, const unsigned int defaultMaterial, const int numCSGBrushes)
{
	auto ctx = GetComputeContext();
	printf("OpenCL device: %s\n", ctx->device.getInfo<CL_DEVICE_NAME>().c_str());
	printf("OpenCL device version: %s\n", ctx->device.getInfo<CL_DEVICE_VERSION>().c_str());
	printf("  Global Memory Size: %d\n", ctx->device.getInfo<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE>());
	printf("  Max Memory Alloc Size: %d\n", ctx->device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>());
	printf("  Max Work Group Size: %d\n", ctx->device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>());
	printf("  Max Work Item Dimensions: %d\n", ctx->device.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>());
	printf("  Min Data Type Align Size: %d\n", ctx->device.getInfo<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>());
	printf("  Memory Base Address Align: %d\n", ctx->device.getInfo<CL_DEVICE_MEM_BASE_ADDR_ALIGN>());
	printf("  Extensions: %s\n", ctx->device.getInfo<CL_DEVICE_EXTENSIONS>().c_str());

	std::stringstream buildOptions;
//	buildOptions << "-cl-nv-verbose ";
	buildOptions << "-cl-denorms-are-zero ";
	buildOptions << "-cl-finite-math-only ";
	buildOptions << "-cl-no-signed-zeros ";
	buildOptions << "-cl-fast-relaxed-math ";
	buildOptions << "-Werror ";
	buildOptions << "-I. ";
	buildOptions << "-DCUCKOO_EMPTY_VALUE=" << CUCKOO_EMPTY_VALUE << " ";
	buildOptions << "-DCUCKOO_STASH_HASH_INDEX=" << CUCKOO_STASH_HASH_INDEX << " ";
	buildOptions << "-DCUCKOO_HASH_FN_COUNT=" << CUCKOO_HASH_FN_COUNT << " ";
	buildOptions << "-DCUCKOO_STASH_SIZE=" << CUCKOO_STASH_SIZE << " ";
	buildOptions << "-DCUCKOO_MAX_ITERATIONS=" << CUCKOO_MAX_ITERATIONS << " ";
	buildOptions << "-DNUM_CSG_BRUSHES=" << numCSGBrushes << " ";
	
	g_utilProgram.initialise("cl/compact.cl", buildOptions.str());
	g_utilProgram.addHeader("cl/duplicate.cl");
	g_utilProgram.addHeader("cl/scan.cl");
	g_utilProgram.addHeader("cl/fill_buffer.cl");
	CL_CALL(g_utilProgram.build());

	ctx->defaultMaterial = defaultMaterial;
	CL_CALL(Compute_InitialiseCuckoo());
	CL_CALL(Compute_SetNoiseSeed(noiseSeed));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_Shutdown()
{
	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

MeshGenerationContext* Compute_CreateMeshGenContext(const int voxelsPerChunk)
{
	MeshGenerationContext* meshGen = new MeshGenerationContext;
	meshGen->voxelsPerChunk = voxelsPerChunk;
	meshGen->hermiteIndexSize = meshGen->voxelsPerChunk + 1;
	meshGen->fieldSize = meshGen->hermiteIndexSize + 1;
	meshGen->indexShift = glm::log2(voxelsPerChunk) + 1;
	meshGen->indexMask = (1 << meshGen->indexShift) - 1;
	const int fieldBufferSize = meshGen->fieldSize * meshGen->fieldSize * meshGen->fieldSize;
	const int maxTerrainHeight = 900.f;

	std::stringstream buildOptions;
//	buildOptions << "-cl-nv-verbose ";
	buildOptions << "-cl-fast-relaxed-math ";
	buildOptions << "-Werror ";
	buildOptions << "-DVOXELS_PER_CHUNK=" << meshGen->voxelsPerChunk << " ";
	buildOptions << "-DLEAF_SIZE_SCALE=" << LEAF_SIZE_SCALE << " ";
	buildOptions << "-DFIELD_DIM=" << meshGen->fieldSize << " ";
	buildOptions << "-DHERMITE_INDEX_SIZE=" << meshGen->hermiteIndexSize << " ";
	buildOptions << "-DVOXEL_INDEX_SHIFT=" << meshGen->indexShift << " ";
	buildOptions << "-DVOXEL_INDEX_MASK=" << meshGen->indexMask << " ";
	buildOptions << "-DMAX_TERRAIN_HEIGHT=" << maxTerrainHeight << " ";
	buildOptions << "-DMATERIAL_AIR=" << MATERIAL_AIR << " ";
	buildOptions << "-DMATERIAL_NONE=" << MATERIAL_NONE << " ";
	buildOptions << "-DFIND_EDGE_INFO_STEPS=" << 16 << " ";
	buildOptions << "-DFIND_EDGE_INFO_INCREMENT=" << (1.f/16.f) << " ";
	buildOptions << "-DMAX_OCTREE_DEPTH=" << glm::log2(meshGen->voxelsPerChunk) << " ";
	buildOptions << "-DCUCKOO_EMPTY_VALUE=" << CUCKOO_EMPTY_VALUE << " ";
	buildOptions << "-DCUCKOO_STASH_HASH_INDEX=" << CUCKOO_STASH_HASH_INDEX << " ";
	buildOptions << "-DCUCKOO_HASH_FN_COUNT=" << CUCKOO_HASH_FN_COUNT << " ";
	buildOptions << "-DCUCKOO_STASH_SIZE=" << CUCKOO_STASH_SIZE << " ";
	buildOptions << "-DCUCKOO_MAX_ITERATIONS=" << CUCKOO_MAX_ITERATIONS << " ";
	buildOptions << "-DFIELD_BUFFER_SIZE=" << fieldBufferSize << " ";
	buildOptions << "-DNUM_CSG_BRUSHES=" << 2 << " ";
	
	meshGen->densityFieldProgram.initialise("cl/density_field.cl", buildOptions.str());
	meshGen->densityFieldProgram.addHeader("cl/shared_constants.cl");
	meshGen->densityFieldProgram.addHeader("cl/simplex.cl");
	meshGen->densityFieldProgram.addHeader("cl/noise.cl");
	if (int error = meshGen->densityFieldProgram.build())
	{
		printf("Error: unable to build density field program for voxelsPerChunk=%d\n  '%s' (%d)\n",
			voxelsPerChunk, GetCLErrorString(error), error);
		return nullptr;
	}

	meshGen->octreeProgram.initialise("cl/octree.cl", buildOptions.str());
	meshGen->octreeProgram.addHeader("cl/shared_constants.cl");
	meshGen->octreeProgram.addHeader("cl/cuckoo.cl");
	meshGen->octreeProgram.addHeader("cl/qef.cl");
	if (int error = meshGen->octreeProgram.build())
	{
		printf("Error: unable to build octree program for voxelsPerChunk=%d\n  '%s' (%d)\n",
			voxelsPerChunk, GetCLErrorString(error), error);
		return nullptr;
	}

	meshGen->csgProgram.initialise("cl/apply_csg_operation.cl", buildOptions.str());
	meshGen->csgProgram.addHeader("cl/shared_constants.cl");
	if (int error = meshGen->csgProgram.build())
	{
		printf("Error: unable to build CSG program for voxelsPerChunk=%d\n  '%s' (%d)\n",
			voxelsPerChunk, GetCLErrorString(error), error);
		return nullptr;
	}

	return meshGen;
}

// ----------------------------------------------------------------------------

int Compute_MeshGenVoxelsPerChunk(MeshGenerationContext* meshGen)
{
	if (meshGen)
	{
		return meshGen->voxelsPerChunk;
	}

	return 0;
}

// ----------------------------------------------------------------------------

int PickScanBlockSize(const int count)
{
	if(count == 0)				{ return 0; }
	else if(count <= 1)	 { return 1; }
	else if(count <= 2)	 { return 2; }
	else if(count <= 4)	 { return 4; }
	else if(count <= 8)	 { return 8; }
	else if(count <= 16)	{ return 16; }
	else if(count <= 32)	{ return 32; }
	else if(count <= 64)	{ return 64; }
	else if(count <= 128) { return 128; }
	else									{ return 256; }
}

// ----------------------------------------------------------------------------

int Scan(cl::CommandQueue& queue, cl::Buffer& data, cl::Buffer& scanData, const int count, const bool exclusive)
{
	const u32 blockSize = PickScanBlockSize(count);
	u32 blockCount = count / blockSize;

	if (blockCount * blockSize < count)
	{
		blockCount++;
	}

	auto ctx = GetComputeContext();
	cl::Buffer blockSums(ctx->context, CL_MEM_READ_WRITE, sizeof(int) * blockCount);
	CL_CALL(FillBufferInt(ctx->queue, blockSums, blockCount, 0));

	const char* scanKernelName = exclusive ? "ExclusiveLocalScan" : "InclusiveLocalScan";
	cl::Kernel localScanKernel(g_utilProgram.get(), scanKernelName);
	CL_CALL(localScanKernel.setArg(0, blockSums));
	CL_CALL(localScanKernel.setArg(1, blockSize * sizeof(int), 0));
	CL_CALL(localScanKernel.setArg(2, blockSize));
	CL_CALL(localScanKernel.setArg(3, count));
	CL_CALL(localScanKernel.setArg(4, data));
	CL_CALL(localScanKernel.setArg(5, scanData));
	CL_CALL(ctx->queue.enqueueNDRangeKernel(localScanKernel, cl::NullRange, blockCount * blockSize, blockSize));

	if (blockCount > 1)
	{
		Scan(ctx->queue, blockSums, blockSums, blockCount, false);

		cl::Kernel writeOutputKernel(g_utilProgram.get(), "WriteScannedOutput");
		writeOutputKernel.setArg(0, scanData);
		writeOutputKernel.setArg(1, blockSums);
		writeOutputKernel.setArg(2, count);

		CL_CALL(ctx->queue.enqueueNDRangeKernel(writeOutputKernel, blockSize, blockCount * blockSize, blockSize));
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int ExclusiveScan(cl::CommandQueue& queue, cl::Buffer& data, cl::Buffer& scan, const u32 count)
{
	auto ctx = GetComputeContext();
	CL_CALL(Scan(ctx->queue, data, scan, count, true));

	int lastValue = 0;
	int lastScanValue = 0;
	CL_CALL(ctx->queue.enqueueReadBuffer(data, CL_FALSE, (count - 1) * sizeof(int), sizeof(int), &lastValue));
	CL_CALL(ctx->queue.enqueueReadBuffer(scan, CL_TRUE, (count - 1) * sizeof(int), sizeof(int), &lastScanValue));

	return lastValue + lastScanValue;
}

// ----------------------------------------------------------------------------

int CompactArray_Long(
	cl::CommandQueue&	queue, 
	cl::Buffer&			valuesArray, 
	cl::Buffer&			validity, 
	const u32		count, 
	cl::Buffer&			compactArray)
{
	auto ctx = GetComputeContext();
	cl::Buffer scan(ctx->context, CL_MEM_READ_WRITE, sizeof(cl_int) * count);
	const int compactCount = ExclusiveScan(ctx->queue, validity, scan, count);

	if (compactCount > 0)
	{
		compactArray = cl::Buffer(ctx->context, CL_MEM_READ_WRITE, sizeof(cl_long) * compactCount);

		cl::Kernel k(g_utilProgram.get(), "CompactArray_Long");
		k.setArg(0, validity);
		k.setArg(1, valuesArray);
		k.setArg(2, scan);
		k.setArg(3, compactArray);
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k, cl::NullRange, count, cl::NullRange));
	}

	return compactCount;
}
// ----------------------------------------------------------------------------

int CompactIndexArray(cl::CommandQueue& queue, cl::Buffer& indexArray, 
					  cl::Buffer& validity, const int count, cl::Buffer& compactArray)
{
	auto ctx = GetComputeContext();
	cl::Buffer scan(ctx->context, CL_MEM_READ_WRITE, count * sizeof(int));
	const int compactCount = ExclusiveScan(ctx->queue, validity, scan, count);

	if (compactCount > 0)
	{
		compactArray = cl::Buffer(ctx->context, CL_MEM_READ_WRITE, compactCount * sizeof(int));
		cl::Kernel k(g_utilProgram.get(), "CompactIndexArray");
		k.setArg(0, validity);
		k.setArg(1, indexArray);
		k.setArg(2, scan);
		k.setArg(3, compactArray);
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k, cl::NullRange, count, cl::NullRange));
	}

	return compactCount;
}

// ----------------------------------------------------------------------------

cl::Buffer RemoveDuplicates(cl::CommandQueue& queue, cl::Buffer& inputData, const int inputCount, unsigned int* resultCount)
{
	rmt_ScopedCPUSample(RemoveDuplicates);

	auto ctx = GetComputeContext();
	cl::Buffer result(ctx->context, CL_MEM_READ_WRITE, inputCount * sizeof(int));
	FillBufferInt(ctx->queue, result, inputCount, -1);

	cl::Buffer sequence(ctx->context, CL_MEM_READ_WRITE, inputCount * sizeof(int));
	ctx->queue.enqueueCopyBuffer(inputData, sequence, 0, 0, inputCount * sizeof(int));

	Timer timer;
	timer.start();
	timer.disable();
//	printf("RemoveDuplicates: inputCount=%d\n", inputCount);

	int prime = FindNextPrime(inputCount * 2);
	cl::Buffer table(ctx->context, CL_MEM_READ_WRITE, prime * sizeof(int));
	cl::Buffer winnersValid(ctx->context, CL_MEM_READ_WRITE, prime * sizeof(int));
	cl::Buffer winners(ctx->context, CL_MEM_READ_WRITE, prime * sizeof(int));
	cl::Buffer losers(ctx->context, CL_MEM_READ_WRITE, prime * sizeof(int));
	cl::Buffer losersValid(ctx->context, CL_MEM_READ_WRITE, prime * sizeof(int));

	cl::Kernel mapSequenceKernel(g_utilProgram.get(), "MapSequenceIndices");
	cl::Kernel extractWinnersKernel(g_utilProgram.get(), "ExtractWinners");
	cl::Kernel mapValuesKernel(g_utilProgram.get(), "MapSequenceValues");
	cl::Kernel extractLosersKernel(g_utilProgram.get(), "ExtractLosers");

	int resultsSize = 0;
	int numItems = inputCount;
	int xorValue = 0xdecb7a37;
	while (numItems > 0)
	{
	//	const int prime = FindNextPrime(numItems * 2);
	//	prime = FindNextPrime(numItems * 2);
	//	printf("  prime=%d\n", prime);
	//	printf("*** xor=%x\n", xorValue);
	//	printf("  edges remaining=%d\n", numItems);
		
		FillBufferInt(ctx->queue, table, prime, -1);

		mapSequenceKernel.setArg(0, sequence);
		mapSequenceKernel.setArg(1, table);
		mapSequenceKernel.setArg(2, prime);
		mapSequenceKernel.setArg(3, xorValue);
		ctx->queue.enqueueNDRangeKernel(mapSequenceKernel, cl::NullRange, numItems, cl::NullRange);

		extractWinnersKernel.setArg(0, table);
		extractWinnersKernel.setArg(1, sequence);
		extractWinnersKernel.setArg(2, winnersValid);
		extractWinnersKernel.setArg(3, winners);
		ctx->queue.enqueueNDRangeKernel(extractWinnersKernel, cl::NullRange, prime, cl::NullRange);

		cl::Buffer compactWinners;
		const int numWinners = CompactIndexArray(ctx->queue, winners, winnersValid, prime, compactWinners);
	//	printf("  %d winners\n", numWinners);

		(ctx->queue.enqueueCopyBuffer(compactWinners, result, 0, resultsSize * sizeof(int), numWinners * sizeof(int)));
		resultsSize += numWinners;

		mapValuesKernel.setArg(0, compactWinners);
		mapValuesKernel.setArg(1, table);
		mapValuesKernel.setArg(2, prime);
		mapValuesKernel.setArg(3, xorValue);
		(ctx->queue.enqueueNDRangeKernel(mapValuesKernel, cl::NullRange, numWinners, cl::NullRange));

		extractLosersKernel.setArg(0, sequence);
		extractLosersKernel.setArg(1, table);
		extractLosersKernel.setArg(2, losersValid);
		extractLosersKernel.setArg(3, losers);
		extractLosersKernel.setArg(4, prime);
		extractLosersKernel.setArg(5, xorValue);
		(ctx->queue.enqueueNDRangeKernel(extractLosersKernel, cl::NullRange, numItems, cl::NullRange));
		
		cl::Buffer compactLosers;
		const int numLosers = CompactIndexArray(ctx->queue, losers, losersValid, numItems, compactLosers);
	//	printf("  %d losers\n", numLosers);
		if (numLosers == 0)
		{
			break;
		}

		numItems = numLosers;
		sequence = compactLosers;

		xorValue = (xorValue * prime) & -1;
		xorValue ^= 0xe93fbc48;
	}

//	printf("  %d ms\n", timer.elapsedMilli());

	*resultCount = resultsSize;
	return std::move(result);
}

// ----------------------------------------------------------------------------

cl::size_t<3> Size3(const u32 size)
{
	cl::size_t<3> s;
	s[0] = size;
	s[1] = size;
	s[2] = size;
	return s;
}

// ----------------------------------------------------------------------------

cl::size_t<3> Size3(const u32 x, const u32 y, const u32 z)
{
	cl::size_t<3> s;
	s[0] = x;
	s[1] = y;
	s[2] = z;
	return s;
}

// ----------------------------------------------------------------------------

cl_int4 LeafScaleVec(const glm::ivec3& v)
{
	cl_int4 s;
	s.x = v.x / LEAF_SIZE_SCALE;
	s.y = v.y / LEAF_SIZE_SCALE;
	s.z = v.z / LEAF_SIZE_SCALE;
	s.w = 0;
	return s;
}

// ----------------------------------------------------------------------------

cl_float4 LeafScaleVec(const glm::vec3& v)
{
	cl_float4 s;
	s.x = v.x / LEAF_SIZE_SCALE;
	s.y = v.y / LEAF_SIZE_SCALE;
	s.z = v.z / LEAF_SIZE_SCALE;
	s.w = 0;
	return s;
}

// ----------------------------------------------------------------------------

cl_float4 LeafScaleVec(const glm::vec4& v)
{
	cl_float4 s;
	s.x = v.x / LEAF_SIZE_SCALE;
	s.y = v.y / LEAF_SIZE_SCALE;
	s.z = v.z / LEAF_SIZE_SCALE;
	s.w = 0;
	return s;
}

// ----------------------------------------------------------------------------

Compute_MeshGenContext* Compute_MeshGenContext::create(const int voxelsPerChunk)
{
	Compute_MeshGenContext* ctx = new Compute_MeshGenContext;
	ctx->privateCtx_ = Compute_CreateMeshGenContext(voxelsPerChunk);
	return ctx;
}

int Compute_MeshGenContext::voxelsPerChunk() const
{
	return privateCtx_->voxelsPerChunk;
}

int Compute_MeshGenContext::applyCSGOperations(
	const std::vector<CSGOperationInfo>& opInfo,
	const glm::ivec3& clipmapNodeMin,
	const int clipmapNodeSize)
{
	return Compute_ApplyCSGOperations(privateCtx_, opInfo, clipmapNodeMin, clipmapNodeSize);
}

int Compute_MeshGenContext::freeChunkOctree(
	const glm::ivec3& min,
	const int size)
{
	return Compute_FreeChunkOctree(privateCtx_, min, size);
}

int Compute_MeshGenContext::isChunkEmpty(
	const glm::ivec3& min,
	const int size,
	bool& isEmpty)
{
	return Compute_ChunkIsEmpty(privateCtx_, min, size, isEmpty);
}

int Compute_MeshGenContext::generateChunkMesh(
	const glm::ivec3& min,
	const int clipmapNodeSize,
	MeshBuffer* meshBuffer,
	std::vector<SeamNodeInfo>& seamNodeBuffer)
{
	return Compute_GenerateChunkMesh(privateCtx_, min, clipmapNodeSize, meshBuffer, seamNodeBuffer);
}

// ----------------------------------------------------------------------------

const char* GetCLErrorString(int error)
{
	switch (error)
	{
		case CL_SUCCESS: return "CL_SUCCESS";
		case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
		case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
		case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
		case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
		case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
		case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
		case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
		case CL_MAP_FAILURE: return "CL_MAP_FAILURE";

		case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
		case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
		case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
		case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
		case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
		case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
		case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
		case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
		case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
		case CL_INVALID_SAMPLER: return "CL_INVALID_SAMPLER";
		case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
		case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
		case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
		case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
		case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
		case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
		case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
		case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
		case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
		case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
		case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
		case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
		case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
		case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
		case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
		case CL_INVALID_GL_OBJECT: return "CL_INVALID_GL_OBJECT";
		case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
		case CL_INVALID_MIP_LEVEL: return "CL_INVALID_MIP_LEVEL";
		case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
		default: return "Unknown OpenCL error code!";
	}
}

