
// ---------------------------------------------------------------------------

uint MurmurHash(const uint value, const uint seed)
{
	const uint c1 = 0xcc9e2d51;
	const uint c2 = 0x1b873593;
	const uint r1 = 15;
	const uint r2 = 13;
	const uint m = 5;
	const uint n = 0xe6546b64;
	
	uint hash = value;
	hash *= c1;
	hash = (hash << r1) | (hash >> (32 - r1));
	hash *= c2;

	hash ^= value;
	hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;

	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);
	
	return hash;
}

// ---------------------------------------------------------------------------

uint DuplicateHash(const uint value, const uint xor, const uint prime)
{ 
	uint hash = MurmurHash(value, xor);
	return hash % prime;
}

// ---------------------------------------------------------------------------

kernel void MapSequenceIndices(
	global int* sequence,
	global int* table,
	const int prime,
	const int xor)
{
	const int id = get_global_id(0);
	const int value = sequence[id];
	const uint hash = DuplicateHash(value, xor, prime);

	table[hash] = id;
}

// ---------------------------------------------------------------------------

kernel void MapSequenceValues(
	global int* sequence,
	global int* table,
	const int prime,
	const int xor)
{
	const int id = get_global_id(0);
	const int value = sequence[id];
	const uint hash = DuplicateHash(value, xor, prime);

	table[hash] = value;
}


// ---------------------------------------------------------------------------

kernel void ExtractWinners(
	global int* table,
	global int* sequence,
	global int* valid,
	global int* winners)
{
	const int id = get_global_id(0);
	const int index = table[id];
	if (index != -1)
	{
		valid[id] = 1;
		winners[id] = sequence[index];
	}
	else
	{
		valid[id] = 0;
		winners[id] = -1;
	}
}

// ---------------------------------------------------------------------------

kernel void ExtractLosers(
	global int* sequence,
	global int* table,
	global int* valid,
	global int* losers,
	const int prime,
	const int xor)
{
	const int id = get_global_id(0);
	const int value = sequence[id];
	const uint hash = DuplicateHash(value, xor, prime);
	if (table[hash] != value)
	{
		valid[id] = 1;
		losers[id] = value;
	}
	else
	{
		valid[id] = 0;
		losers[id] = -1;
	}
}

// ---------------------------------------------------------------------------

