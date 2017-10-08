#include	"compute_cuckoo.h"

#include	"compute_local.h"
#include	"compute_program.h"
#include	"primes.h"

#include	<random>
#include	<sstream>

// ----------------------------------------------------------------------------

cl::Kernel	k_InsertKeys;

// enforce a minimum size when the table has a small size to avoid collisions
const unsigned int MIN_TABLE_SIZE = 2048U;		

// ----------------------------------------------------------------------------

int Compute_InitialiseCuckoo()
{
	std::stringstream buildOptions;
	buildOptions << "-DCUCKOO_EMPTY_VALUE=" << CUCKOO_EMPTY_VALUE << " ";
	buildOptions << "-DCUCKOO_STASH_HASH_INDEX=" << CUCKOO_STASH_HASH_INDEX << " ";
	buildOptions << "-DCUCKOO_HASH_FN_COUNT=" << CUCKOO_HASH_FN_COUNT << " ";
	buildOptions << "-DCUCKOO_STASH_SIZE=" << CUCKOO_STASH_SIZE << " ";
	buildOptions << "-DCUCKOO_MAX_ITERATIONS=" << CUCKOO_MAX_ITERATIONS;

	ComputeProgram program;
	program.initialise("cl/cuckoo.cl", buildOptions.str());
	CL_CALL(program.build());
	
	cl_int error = CL_SUCCESS;
	k_InsertKeys = cl::Kernel(program.get(), "Cuckoo_InsertKeys", &error);
	if (!k_InsertKeys() || error != CL_SUCCESS)
	{
		printf("Error! Failed to create 'InsertKeys' kernel: %s (%d)",
			GetCLErrorString(error), error);
		return error;
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

std::mt19937 generator;
std::uniform_int_distribution<uint32_t> distribution(1 << 15, 1 << 30);

int Cuckoo_InitialiseTable(CuckooData* data, const unsigned int tableSize)
{
	auto ctx = GetComputeContext();

	data->prime = FindNextPrime(glm::max(MIN_TABLE_SIZE, tableSize * 2));

	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, data->prime * sizeof(uint64_t), nullptr, data->table));
	CL_CALL(FillBufferLong(ctx->queue, data->table, data->prime, CUCKOO_EMPTY_VALUE));

	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, CUCKOO_STASH_SIZE * sizeof(uint64_t), nullptr, data->stash));
	CL_CALL(FillBufferLong(ctx->queue, data->stash, CUCKOO_STASH_SIZE, CUCKOO_EMPTY_VALUE));

	uint32_t params[CUCKOO_HASH_FN_COUNT * 2];
	for (int i = 0; i < CUCKOO_HASH_FN_COUNT; i++)
	{
		params[i * 2 + 0] = distribution(generator);
		params[i * 2 + 1] = distribution(generator);
	}

	CL_CALL(CreateBuffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
		sizeof(uint32_t) * CUCKOO_HASH_FN_COUNT * 2, &params[0], data->hashParams));

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int	Cuckoo_InsertKeys(CuckooData* data, const cl::Buffer& d_keys, const unsigned int count)
{
	ComputeContext* ctx = GetComputeContext();

	cl::Buffer d_inserted, d_stashUsed;
	cl::Buffer d_insertedScan, d_stashUsedScan;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(int) * count, nullptr, d_inserted));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(int) * count, nullptr, d_stashUsed));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(int) * count, nullptr, d_insertedScan));
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE, sizeof(int) * count, nullptr, d_stashUsedScan));

	int numRetries = 0;
	int insertedCount = 0, stashUsedCount = 0;
	do
	{
		if (insertedCount > 0)
		{
			printf("Cuckoo: insert keys failed (failed to insert %d key from %d). Retry #%d...\n", count - insertedCount, count, ++numRetries);

			uint32_t params[CUCKOO_HASH_FN_COUNT * 2];
			for (int i = 0; i < CUCKOO_HASH_FN_COUNT; i++)
			{
				params[i * 2 + 0] = distribution(generator);
				params[i * 2 + 1] = distribution(generator);
			}

			CL_CALL(ctx->queue.enqueueWriteBuffer(
				data->hashParams, CL_FALSE, 0, sizeof(u32) * CUCKOO_HASH_FN_COUNT * 2, &params[0]));
			CL_CALL(FillBufferLong(ctx->queue, data->table, data->prime, CUCKOO_EMPTY_VALUE));
			CL_CALL(FillBufferLong(ctx->queue, data->stash, CUCKOO_STASH_SIZE, CUCKOO_EMPTY_VALUE));
		}

		int index = 0;
		CL_CALL(k_InsertKeys.setArg(index++, d_keys));
		CL_CALL(k_InsertKeys.setArg(index++, data->table));
		CL_CALL(k_InsertKeys.setArg(index++, data->stash));
		CL_CALL(k_InsertKeys.setArg(index++, data->prime));
		CL_CALL(k_InsertKeys.setArg(index++, data->hashParams));
		CL_CALL(k_InsertKeys.setArg(index++, d_inserted));
		CL_CALL(k_InsertKeys.setArg(index++, d_stashUsed));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_InsertKeys, cl::NullRange, count, cl::NullRange));

		insertedCount = ExclusiveScan(ctx->queue, d_inserted, d_insertedScan, count);
		if (insertedCount < 0)
		{
			// i.e. an error
			return insertedCount;
		}

		stashUsedCount = ExclusiveScan(ctx->queue, d_stashUsed, d_stashUsedScan, count);
		if (stashUsedCount < 0)
		{
			// i.e. an error
			return stashUsedCount;
		}
	}
	while (insertedCount < count);

	data->stashUsed |= stashUsedCount != 0;
	data->insertedKeys += insertedCount;

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------
