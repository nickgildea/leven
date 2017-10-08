#include	<catch.hpp>

#include	"cuckoo.h"

TEST_CASE("CuckooHashTable", "[hashtable]")
{
	struct TestData
	{
		int				numUniqueKeys;
		int				hashTableSize;
		float			minInsertSuccess;
	};

	const int TEST_SIZE_SMALL = 1 << 5;
	const int TEST_SIZE_MEDIUM = 1 << 14;
	const int TEST_SIZE_LARGE = 1 << 20;

	const float NORMAL_SUCCESS_RATIO = 0.98f;
	const float HIGH_SUCCESS_RATIO = 0.99f;

	const TestData tests[] =
	{
		{ TEST_SIZE_SMALL, TEST_SIZE_SMALL, NORMAL_SUCCESS_RATIO },
		{ TEST_SIZE_MEDIUM, TEST_SIZE_MEDIUM, NORMAL_SUCCESS_RATIO },
		{ TEST_SIZE_LARGE, TEST_SIZE_LARGE, NORMAL_SUCCESS_RATIO },

		{ TEST_SIZE_SMALL, TEST_SIZE_SMALL, NORMAL_SUCCESS_RATIO },
		{ TEST_SIZE_SMALL, TEST_SIZE_MEDIUM, HIGH_SUCCESS_RATIO },
		{ TEST_SIZE_SMALL, TEST_SIZE_LARGE, 1.f },

		{ TEST_SIZE_MEDIUM, TEST_SIZE_SMALL, 0.01f },
		{ TEST_SIZE_MEDIUM, TEST_SIZE_MEDIUM, NORMAL_SUCCESS_RATIO },
		{ TEST_SIZE_MEDIUM, TEST_SIZE_LARGE, HIGH_SUCCESS_RATIO },
		{ -1, -1, false }
	};

	INFO("CuckooHashTable test...");

	std::mt19937 generator;
	std::uniform_int_distribution<uint32_t> distribution;

	std::unordered_set<uint32_t> testKeys;
	INFO("Generating " << TEST_SIZE_LARGE << " unique keys");
	while (testKeys.size() < TEST_SIZE_LARGE)
	{
		uint32_t k = distribution(generator);
		if (testKeys.find(k) == end(testKeys))
		{
			testKeys.insert(k);
		}
	}

	int counter = 0;
	for (auto test = tests; test->numUniqueKeys != -1; test++, counter++)
	{
		INFO("Test case: " << counter);

		CuckooHashTable cuckoo(test->hashTableSize);

		int numInserted = 0;
		auto iter = begin(testKeys);
		for (int i = 0; i < test->numUniqueKeys; i++)
		{
			const uint32_t k = *iter;
			iter++;

			if (cuckoo.insert(k, 0))
			{
				numInserted++;
			}
		}

		const float insertRatio = (float)numInserted / (float)test->numUniqueKeys;
		REQUIRE(insertRatio >= test->minInsertSuccess);

		// for test where most values will not inserted this test won't work
		// as a lot of the "successfully" inserted values actually end up in the 
		// stash and get evicted during the unsuccessful insert operations
		if (test->minInsertSuccess >= NORMAL_SUCCESS_RATIO)
		{
			int numFound = 0;
			int numCorrectValue = 0;
			iter = begin(testKeys);
			for (int i = 0; i < test->numUniqueKeys; i++)
			{
				const uint32_t k = *iter;
				iter++;

				uint32_t value = ~0;
				if (cuckoo.find(k, &value))
				{
					if (value == 0)
					{
						numCorrectValue++;
					}

					numFound++;
				}
			}

			REQUIRE(numFound == numInserted);
			REQUIRE(numFound == numCorrectValue);
		}
	}
}

#include "testdata/octree_keys_184.cpp"
#include "testdata/octree_keys_168.cpp"
#include "testdata/octree_keys_146.cpp"
#include "testdata/octree_keys_141.cpp"
#include "testdata/octree_keys_136.cpp"
#include "testdata/octree_keys_122.cpp"
#include "testdata/octree_keys_119.cpp"
#include "testdata/octree_keys_109.cpp"
#include "testdata/octree_keys_91.cpp"
#include "testdata/octree_keys_42.cpp"
#include "testdata/octree_keys_28.cpp"
#include "testdata/octree_keys_3.cpp"

TEST_CASE("CuckooHashTable (Octree data)", "[hashtable] [octree]")
{
	struct TestData
	{
		const uint32_t*	   keys;
		uint32_t           count;
	};

	const TestData testSets[] =
	{
		{ OCTREE_KEYS_184, sizeof(OCTREE_KEYS_184) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_168, sizeof(OCTREE_KEYS_168) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_146, sizeof(OCTREE_KEYS_146) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_141, sizeof(OCTREE_KEYS_141) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_136, sizeof(OCTREE_KEYS_136) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_122, sizeof(OCTREE_KEYS_122) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_119, sizeof(OCTREE_KEYS_119) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_109, sizeof(OCTREE_KEYS_109) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_91, sizeof(OCTREE_KEYS_91) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_42, sizeof(OCTREE_KEYS_42) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_28, sizeof(OCTREE_KEYS_28) / sizeof(uint32_t) }, 
		{ OCTREE_KEYS_3, sizeof(OCTREE_KEYS_3) / sizeof(uint32_t) }, 
		{ nullptr, 0 },
	};

	for (int test = 0; testSets[test].keys != nullptr; test++)
	{
		INFO("Test: " << test);
		CuckooHashTable cuckoo(testSets[test].count);

		int totalCount = 0, insertedCount = 0;
		for (const uint32_t* code = testSets[test].keys; *code != ~0; code++)
		{
			if (cuckoo.insert(*code, 42))
			{
				insertedCount++;
			}
			else
			{
				WARN("Failed to insert code: " << *code);
			}

			totalCount++;
		}

		REQUIRE(totalCount == insertedCount);

		bool allFound = true;
		bool all42 = true;
		for (const uint32_t* code = testSets[test].keys; *code != ~0; code++)
		{
			uint32_t value = ~0;
			allFound = allFound && cuckoo.find(*code, &value);
			all42 = all42 && value == 42;
		}

		REQUIRE(allFound);
		REQUIRE(all42);
	}
}