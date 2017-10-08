#ifndef 	HAS_COMPUTE_CUCKOO_BEEN_INCLUDED
#define		HAS_COMPUTE_CUCKOO_BEEN_INCLUDED

#include	<CL/cl.hpp>

const uint64_t CUCKOO_EMPTY_VALUE = ~0ULL;
const int      CUCKOO_STASH_HASH_INDEX = 4;
const int      CUCKOO_HASH_FN_COUNT = CUCKOO_STASH_HASH_INDEX + 1;
const int      CUCKOO_STASH_SIZE = 101;
const int      CUCKOO_MAX_ITERATIONS = 32;

struct CuckooData
{
	cl::Buffer			table, stash;
	cl::Buffer			hashParams;
	int					prime = -1;
	int					insertedKeys = 0;
	int					stashUsed = 0;					// int rather than bool as bools seem somewhat iffy in OpenCL
};

int Compute_InitialiseCuckoo();

int Cuckoo_InitialiseTable(CuckooData* data, const unsigned int tableSize);
int	Cuckoo_InsertKeys(CuckooData* data, const cl::Buffer& d_keys, const unsigned int count);

#endif  // HAS_COMPUTE_CUCKOO_BEEN_INCLUDED
