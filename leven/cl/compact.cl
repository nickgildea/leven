
// ---------------------------------------------------------------------------

__kernel void CompactIndexArray(
	__global int* edgeValid,
	__global int* edgeIndices,
	__global int* scan,
	__global int* compactIndices
)
{
	const int index = get_global_id(0);
	if (edgeValid[index])
	{
		compactIndices[scan[index]] = edgeIndices[index];
	}
}

// ---------------------------------------------------------------------------

__kernel void CompactArray_Long(
	__global int* valid,
	__global long* values,
	__global int* scan,
	__global long* compactValues
)
{
	const int index = get_global_id(0);
	if (valid[index])
	{
		compactValues[scan[index]] = values[index];
	}
}

// ---------------------------------------------------------------------------
