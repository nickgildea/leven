#ifndef		HAS_CUCKOO_H_BEEN_INCLUDED
#define		HAS_CUCKOO_H_BEEN_INCLUDED

// based on the GPU hash table described in "Efficient Hash Tables on the GPU" by Dan Alcaranta
// http://idav.ucdavis.edu/~dfalcant/downloads/dissertation.pdf

#include	<stdint.h>
#include	<vector>
#include	<random>
#include	<unordered_set>

#include	"primes.h"

class CuckooHashTable
{
public:

	CuckooHashTable(const int size, const uint32_t seed=0x4be3f)
		: insertedKeys_(0)
		, stashUsed_(false)
	{
		int prime = FindNextPrime(size * 2.f);
		data_.resize(prime, EMPTY_VALUE);

		for (int i = 0; i < STASH_SIZE; i++)
		{
			stash_[i] = EMPTY_VALUE;
		}

		std::mt19937 generator;
		generator.seed(seed);
		std::uniform_int_distribution<uint32_t> distribution(1 << 10, 1 << 20);

		for (int i = 0; i < HASH_COUNT; i++)
		for (int j = 0; j < 2; j++)
		{
			hashParams_[i][j] = distribution(generator);
		}
	}

	bool insert(const uint32_t key, const uint32_t value)
	{
		uint64_t entry = createEntry(key, value);
		uint32_t h = hash(0, key);

		for (int i = 0; i < MAX_ITERATIONS; i++)
		{
			std::swap(data_[h], entry);
			if (entry == EMPTY_VALUE)
			{
				insertedKeys_++;
				return true;
			}

			// failed, find a new slot for the evicted value
			const uint32_t h0 = hash(0, getKey(entry));
			const uint32_t h1 = hash(1, getKey(entry));
			const uint32_t h2 = hash(2, getKey(entry));
			const uint32_t h3 = hash(3, getKey(entry));

			     if (h == h0) { h = h1; }
			else if (h == h1) { h = h2; }
			else if (h == h2) { h = h3; }
			else if (h == h3) { h = h0; }
		}

		// exceeded max iterations, attempt the stash
		stashUsed_ = true;
		h = hash(STASH_HASH, getKey(entry));
		if (stash_[h] == EMPTY_VALUE)
		{
			stash_[h] = entry;
			insertedKeys_++;
			return true;
		}

		return false;
	}

	bool find(const uint32_t key, uint32_t* value) const
	{
		for (int i = 0; i < STASH_HASH; i++)
		{
			const uint32_t h = hash(i, key);
			if (getKey(data_[h]) == key)
			{
				*value = getValue(data_[h]);
				return true;
			}
		}

		if (stashUsed_)
		{
			const uint32_t h = hash(STASH_HASH, key);
			if (getKey(stash_[h]) == key)
			{
				*value = getValue(stash_[h]);
				return true;
			}
		}

		return false;
	}

	bool hasKey(const uint32_t key) const
	{
		uint32_t value = ~0;
		return find(key, &value);
	}

private:

	const uint64_t EMPTY_VALUE = ~0ULL;

	uint64_t createEntry(const uint32_t key, const uint32_t value) const
	{
		return (uint64_t)(((uint64_t)value << 32) | key);
	}

	uint32_t getKey(const uint64_t n) const
	{
		return n & 0xffffffff;
	}

	uint32_t getValue(const uint64_t n) const
	{
		return (n >> 32) & 0xffffffff;
	}

	uint32_t MWC64X(uint64_t *state)
	{
		uint32_t c=(*state)>>32, x=(*state)&0xFFFFFFFF;
		*state = x*((uint64_t)4294883355U) + c;
		return x^c;
	}

	uint32_t hash(int whichHash, const uint64_t key) const
	{
		LVN_ASSERT(whichHash < HASH_COUNT);
		uint32_t mod = whichHash < 4 ? data_.size() : STASH_SIZE;
#if 0
		uint32_t c = hashParams_[whichHash][0];
		uint32_t x = hashParams_[whichHash][1];
		return ((x ^ c) ^ key) % mod;
#else
		const uint32_t a = hashParams_[whichHash][0];
		const uint32_t b = hashParams_[whichHash][1];
		const uint32_t p = 4294967291;		// largest 32-bit prime

		uint64_t h = key * a;
		return ((h + b) % p) % mod;
#endif
	}

	static const int STASH_HASH = 4;
	static const int HASH_COUNT = STASH_HASH + 1;
	static const int STASH_SIZE = 101;
	static const int MAX_ITERATIONS = 32;

	int							insertedKeys_;
	uint32_t					hashParams_[HASH_COUNT][2];
	bool						stashUsed_;
	uint64_t					stash_[STASH_SIZE];
	std::vector<uint64_t>		data_;
};

#endif	//	HAS_CUCKOO_H_BEEN_INCLUDED
