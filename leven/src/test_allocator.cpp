#include	"pool_allocator.h"

#include	<catch.hpp>
#include	<random>
#include	<vector>
#include	<unordered_set>

TEST_CASE("Allocator (Pool)", "[allocator] [pool_allocator]")
{
	std::mt19937 prng;

	PoolAllocator<int> pool;

	REQUIRE(pool.initialise(-1) == LVN_ERR_INVALID_PARAM_SIZE);
	REQUIRE(pool.initialise(8) == LVN_SUCCESS);
	REQUIRE(pool.free(nullptr) == LVN_ERR_INVALID_PARAM_PTR);

	const int numRuns = 5;
	int maxItems = 256;
	for (int run = 0; run < numRuns; run++)
	{
		pool.clear();

		pool.initialise(maxItems);
		REQUIRE(pool.size() == 0);
		
		// should be able to alloc maxItems indices
		std::unordered_set<int*> allocatedPtrs;
		for (int i = 0; i < maxItems; i++)
		{
			int* data = pool.alloc();
			REQUIRE(data != nullptr);
			allocatedPtrs.insert(data);
		}

		// all data elements should have been touched
		REQUIRE(allocatedPtrs.size() == maxItems);

		// alloc should fail
		for (int i = 0; i < 5; i++)
		{
			int* invalidIndex = pool.alloc();
			REQUIRE(invalidIndex == nullptr);
		}

		// free all the items
		for (int* p: allocatedPtrs)
		{
			pool.free(p);
		}
		allocatedPtrs.clear();

		REQUIRE(pool.size() == 0);

		for (int pass = 0; pass < 3; pass++)
		{
			std::uniform_int_distribution<int> allocSizeRandom(8, maxItems / 2);
			const int allocSize = allocSizeRandom(prng);
			std::unordered_set<int*> allocatedPtrs;
			for (int i = 0; i < allocSize; i++)
			{
				int* p = pool.alloc();
				REQUIRE(p != nullptr);
				allocatedPtrs.insert(p);
			}

			REQUIRE(pool.size() == allocSize);
			REQUIRE(allocatedPtrs.size() == allocSize);

			std::uniform_int_distribution<int> freeSizeRandom(1, allocSize);
			const int freeSize = freeSizeRandom(prng);
			auto iter = begin(allocatedPtrs);
			for (int i = 0; i < freeSize; i++)
			{
				int* p = *iter;
				pool.free(p);

				iter = allocatedPtrs.erase(iter);
			}

			// do another round of allocations after the frees
			for (int i = 0; i < allocSize; i++)
			{
				int* p = pool.alloc();
				REQUIRE(p != nullptr);
				allocatedPtrs.insert(p);
			}

			// look at the data and check we have the correct number of allocations
			const int finalAllocCount = allocSize - freeSize + allocSize;

			REQUIRE(allocatedPtrs.size() == finalAllocCount);
			REQUIRE(allocatedPtrs.size() == pool.size());

			// reset the data
			pool.clear();
			pool.initialise(maxItems);
		}

		maxItems <<= 1;
	}
}

TEST_CASE("Allocator (Index Pool)", "[allocator] [index_pool_allocator]")
{
	IndexPoolAllocator indexPool;

	std::mt19937 prng;

	const int DATA_ALLOCED = 0x42424242;
	const int DATA_FREED = 0xcdcdcdcd;
	
	const int numRuns = 5;
	int poolSize = 256;
	for (int run = 0; run < numRuns; run++)
	{
		std::vector<int> data(poolSize, DATA_FREED);

		indexPool.clear();
		indexPool.initialise(poolSize);
		REQUIRE(indexPool.size() == 0);
		
		// should be able to alloc poolSize indices
		for (int i = 0; i < poolSize; i++)
		{
			const int index = indexPool.alloc();
			const bool indexValid = index >= 0 && index < poolSize;
			REQUIRE(indexValid);
			data[index] = DATA_ALLOCED;
		}

		// all data elements should have been touched
		for (int i = 0; i < poolSize; i++)
		{
			REQUIRE(data[i] == DATA_ALLOCED);
		}

		// alloc should fail
		for (int i = 0; i < 5; i++)
		{
			const int invalidIndex = indexPool.alloc();
			REQUIRE(invalidIndex == -1);
		}

		// free all the items
		for (int i = 0; i < poolSize; i++)
		{
			int index = i;
			data[index] = DATA_FREED;
			indexPool.free(&index);
		}

		REQUIRE(indexPool.size() == 0);

		for (int pass = 0; pass < 3; pass++)
		{
			std::uniform_int_distribution<int> allocSizeRandom(8, poolSize / 2);
			const int allocSize = allocSizeRandom(prng);
			std::vector<int> indices(allocSize, -1);
			for (int i = 0; i < allocSize; i++)
			{
				indices[i] = indexPool.alloc();
				REQUIRE(data[indices[i]] == DATA_FREED);
				data[indices[i]] = DATA_ALLOCED;
			}

			REQUIRE(indexPool.size() == allocSize);

			std::random_shuffle(begin(indices), end(indices));

			std::uniform_int_distribution<int> freeSizeRandom(1, allocSize);
			const int freeSize = freeSizeRandom(prng);
			for (int i = 0; i < freeSize; i++)
			{
				REQUIRE(data[indices[i]] == DATA_ALLOCED);
				data[indices[i]] = DATA_FREED;
				indexPool.free(&indices[i]);
				REQUIRE(indices[i] == -1);
			}

			// do another round of allocations after the frees
			for (int i = 0; i < allocSize; i++)
			{
				const int index = indexPool.alloc();
				REQUIRE(data[index] == DATA_FREED);
				data[index] = DATA_ALLOCED;
			}

			// look at the data and check we have the correct number of allocations
			const int finalAllocCount = allocSize - freeSize + allocSize;
			int numAlloced = 0;
			for (int i = 0; i < poolSize; i++)
			{
				if (data[i] == DATA_ALLOCED)
				{
					numAlloced++;
				}
			}

			REQUIRE(numAlloced == finalAllocCount);
			REQUIRE(numAlloced == indexPool.size());

			// reset the data
			indexPool.clear();
			indexPool.initialise(poolSize);

			for (int i = 0; i < poolSize; i++)
			{
				data[i] = DATA_FREED;
			}
		}


		poolSize <<= 1;
	}
}