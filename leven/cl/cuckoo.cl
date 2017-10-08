#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

unsigned long Cuckoo_CreateEntry(uint key, uint value)
{
	return (unsigned long)(((unsigned long)value << 32) | key);
}

uint Cuckoo_GetKey(unsigned long entry)
{
	return entry & 0xffffffff;
}

uint Cuckoo_GetValue(unsigned long entry)
{
	return entry >> 32;
}

uint Cuckoo_Hash(int whichHash, uint key, uint a, uint b, uint prime)
{
	const uint p = 4294967291;		// largest 32-bit prime
	unsigned long h = a * key;
	uint mod = whichHash < CUCKOO_STASH_HASH_INDEX ? prime : CUCKOO_STASH_SIZE;
	return ((h + b) % p) % mod;
}

kernel void Cuckoo_InsertKeys(
	global uint* keys,
	global unsigned long* data,
	global unsigned long* stash,
	const uint prime,
	global uint* hashParams,
	global int* inserted,
	global int* stashUsed)
{
	const int index = get_global_id(0);
	uint key = keys[index];
	uint value = index;
	unsigned long entry = Cuckoo_CreateEntry(key, value);

	uint h = Cuckoo_Hash(0, key, hashParams[0 * 2 + 0], hashParams[0 * 2 + 1], prime);
#pragma unroll
	for (int i = 0; i < CUCKOO_MAX_ITERATIONS; i++)
	{
		entry = atom_xchg(&data[h], entry);
		if (entry == CUCKOO_EMPTY_VALUE)
		{
			// swapped with empty value, nothing more to do
			inserted[index] = 1;
			stashUsed[index] = 0;
			return;
		}

		key = Cuckoo_GetKey(entry);
		const uint h0 = Cuckoo_Hash(0, key, hashParams[0 * 2 + 0], hashParams[0 * 2 + 1], prime);
		const uint h1 = Cuckoo_Hash(1, key, hashParams[1 * 2 + 0], hashParams[1 * 2 + 1], prime);
		const uint h2 = Cuckoo_Hash(2, key, hashParams[2 * 2 + 0], hashParams[2 * 2 + 1], prime);
		const uint h3 = Cuckoo_Hash(3, key, hashParams[3 * 2 + 0], hashParams[3 * 2 + 1], prime);

		     if (h == h0) { h = h1; }
		else if (h == h1) { h = h2; }
		else if (h == h2) { h = h3; }
		else if (h == h3) { h = h0; }
	}

	stashUsed[index] = 1;
	key = Cuckoo_GetKey(entry);
	h = Cuckoo_Hash(0, key, hashParams[CUCKOO_STASH_HASH_INDEX * 2 + 0], 
		hashParams[CUCKOO_STASH_HASH_INDEX * 2 + 1], prime);
	const unsigned long stashEntry = atom_cmpxchg(&stash[h], CUCKOO_EMPTY_VALUE, entry);
	inserted[index] = stashEntry == CUCKOO_EMPTY_VALUE ? 1 : 0;
}

uint Cuckoo_Find(
	uint key,
	global unsigned long* data,
	global unsigned long* stash,
	const uint prime,
	global uint* hashParams,
	const int stashUsed)
{
#pragma unroll
	for (int i = 0; i < CUCKOO_STASH_HASH_INDEX; i++)
	{
		const uint h = Cuckoo_Hash(i, key, hashParams[i * 2 + 0], hashParams[i * 2 + 1], prime);
		const unsigned long entry = data[h];
		if (Cuckoo_GetKey(entry) == key)
		{
			return Cuckoo_GetValue(entry);
		}
	}

	if (stashUsed)
	{
		const uint h = Cuckoo_Hash(CUCKOO_STASH_HASH_INDEX, key, 
			hashParams[CUCKOO_STASH_HASH_INDEX * 2 + 0], hashParams[CUCKOO_STASH_HASH_INDEX * 2 + 1], prime);
		const unsigned long entry = data[h];
		if (Cuckoo_GetKey(entry) == key)
		{
			return Cuckoo_GetValue(entry);
		}
	}

	return ~0;
}

