
kernel void ExclusiveLocalScan(
	global int* block_sums, 
	local int* scratch, 
	const uint block_size, 
	const uint count, 
	global int* data, 
	global int* scan_data
	)
{
	const uint gid = get_global_id(0);
	const uint lid = get_local_id(0);

	if (gid < count)
	{
		if (lid == 0)
		{ 
			scratch[lid] = 0;
		}
		else 
		{ 
			scratch[lid] = data[gid-1]; 
		}
	}
	else 
	{
		scratch[lid] = 0;
	}

	barrier(CLK_LOCAL_MEM_FENCE);
	
	for (uint i = 1; i < block_size; i <<= 1)
	{
		const int x = lid >= i ? scratch[lid-i] : 0;

		barrier(CLK_LOCAL_MEM_FENCE);

		if (lid >= i)
		{
			scratch[lid] = scratch[lid] + x;
		}

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	if (gid < count)
	{
		scan_data[gid] = scratch[lid];
	}

	if (lid == block_size - 1 && gid < count)
	{
		block_sums[get_group_id(0)] = data[gid] + scratch[lid];
	}
}

// ---------------------------------------------------------------------------

kernel void InclusiveLocalScan(
	global int* block_sums, 
	local int* scratch, 
	const uint block_size, 
	const uint count, 
	global int* data,
	global int* scan_data)
{
	const uint gid = get_global_id(0);
	const uint lid = get_local_id(0);

	if (gid < count)
	{
		scratch[lid] = data[gid];
	}
	else 
	{
		scratch[lid] = 0;
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (uint i = 1; i < block_size; i <<= 1)
	{
		const int x = lid >= i ? scratch[lid-i] : 0;

		barrier(CLK_LOCAL_MEM_FENCE);
		
		if (lid >= i)
		{
			scratch[lid] = scratch[lid] + x;
		}
		
		barrier(CLK_LOCAL_MEM_FENCE);
	}

	if (gid < count) 
	{
		scan_data[gid] = scratch[lid];
	}

	if (lid == block_size - 1)
	{
		block_sums[get_group_id(0)] = scratch[lid];
	}
}

// ---------------------------------------------------------------------------

kernel void WriteScannedOutput(
	global int* output, 
	global int* block_sums, 
	const uint count
	)
{
	const uint gid = get_global_id(0);
	const uint block_id = get_group_id(0);

	if (gid < count)
	{
		output[gid] += block_sums[block_id];
	}
}

// ---------------------------------------------------------------------------


