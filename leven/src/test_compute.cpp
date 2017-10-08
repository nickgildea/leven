#include	"catch.hpp"
#include	"compute.h"
#include	"compute_local.h"
#include	"compute_cuckoo.h"
#include	"compute_sort.h"
#include	"timer.h"
#include	"volume_constants.h"

#include	"testdata/octree_keys_3.cpp"
#include	"testdata/duplicate_data_3.cpp"

#include	<random>

#define CL_REQUIRE(f) REQUIRE((f) == CL_SUCCESS)

int EnsureComputeInitialised()
{
	static bool s_initialised = false;
	if (!s_initialised)
	{
		s_initialised = true;
		return Compute_Initialise(0x7d3af, 0, 2);
	}

	return CL_SUCCESS;
}

int TestRemoveDuplicate(
	ComputeContext* ctx,
	const uint32_t* values, 
	uint32_t count, 
	std::vector<uint32_t>& uniqueValues)
{
	cl::Buffer d_values;
	CL_CALL(CreateBuffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint32_t) * count, (void*)values, d_values));

	unsigned int uniqueCount = 0;
	cl::Buffer d_uniqueValues = RemoveDuplicates(ctx->queue, d_values, count, &uniqueCount);
	
	uniqueValues.resize(uniqueCount);
	CL_CALL(ctx->queue.enqueueReadBuffer(d_uniqueValues, CL_TRUE, 0, sizeof(uint32_t) * uniqueCount, &uniqueValues[0]));

	return CL_SUCCESS;
}

TEST_CASE("Compute (Remove Duplicates)", "[compute]")
{
	REQUIRE(EnsureComputeInitialised() == CL_SUCCESS);
	auto ctx = GetComputeContext();

	const uint32_t* values = OCTREE_KEYS_DUPLICATE_3;
	const uint32_t count = (sizeof(OCTREE_KEYS_DUPLICATE_3) / sizeof(uint32_t)) - 1;

	std::vector<uint32_t> uniqueValues;
	REQUIRE(TestRemoveDuplicate(ctx, values, count, uniqueValues) == CL_SUCCESS);

	const uint32_t* uniques = OCTREE_KEYS_3;
	const uint32_t uniqueCount = (sizeof(OCTREE_KEYS_3) / sizeof(uint32_t)) - 1;
	REQUIRE(uniqueCount == uniqueValues.size());

	std::vector<uint32_t> uniqueReference;
	uniqueReference.insert(end(uniqueReference), &uniques[0], &uniques[uniqueCount]);

	std::sort(begin(uniqueValues), end(uniqueValues));
	std::sort(begin(uniqueReference), end(uniqueReference));

	REQUIRE(memcmp(&uniqueReference[0], &uniqueValues[0], sizeof(uint32_t) * uniqueCount) == 0);
}

TEST_CASE("Compute (Cuckoo)", "[compute] [hashtable]")
{
	REQUIRE(EnsureComputeInitialised() == CL_SUCCESS);

	const int KEY_COUNT = 100;
	std::vector<uint32_t> keys(KEY_COUNT);
	for (int i = 0; i < 100; i++)
	{
		keys[i] = i;
	}

	cl::Buffer d_keys;
	REQUIRE(CreateBuffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 
		sizeof(uint32_t) * KEY_COUNT, &keys[0], d_keys) == CL_SUCCESS);	

	CuckooData cuckooData;
	REQUIRE(Cuckoo_InitialiseTable(&cuckooData, KEY_COUNT) == CL_SUCCESS);
	REQUIRE(Cuckoo_InsertKeys(&cuckooData, d_keys, KEY_COUNT) == CL_SUCCESS);
}
