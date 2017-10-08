
// ---------------------------------------------------------------------------

kernel void FillBufferInt(
	global int* buffer,
	const int value)
{
	const int index = get_global_id(0);
	if (index < get_global_size(0))
	{
		buffer[index] = value;
	}
}

kernel void FillBufferLong(
	global long* buffer,
	const long value)
{
	const int index = get_global_id(0);
	if (index < get_global_size(0))
	{
		buffer[index] = value;
	}
}


// ---------------------------------------------------------------------------

kernel void FillBufferFloat(
	global float* buffer,
	const float value)
{
	const int index = get_global_id(0);
	if (index < get_global_size(0))
	{
		buffer[index] = value;
	}
}

// ---------------------------------------------------------------------------

