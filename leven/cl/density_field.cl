
constant int4 EDGE_END_OFFSETS[3] =
{
	(int4)(1, 0, 0, 0),
	(int4)(0, 1, 0, 0),
	(int4)(0, 0, 1, 0),
};

// ---------------------------------------------------------------------------

kernel void GenerateDefaultField(
	read_only image2d_t permTexture,
	const int4 offset,
	const int sampleScale,
	const int defaultMaterialIndex,
	global int* field_materials)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);
	const int z = get_global_id(2);

	const float4 world_pos = 
	{ 
		(x * sampleScale) + offset.x, 
		(y * sampleScale) + offset.y, 
		(z * sampleScale) + offset.z,
		0
	};

	const float density = DensityFunc(world_pos, permTexture);

	const int4 local_pos = { x, y, z, 0 };
	const int index = field_index(local_pos);
	const int material = density < 0.f ? defaultMaterialIndex : MATERIAL_AIR;

	field_materials[index] = material;
}

// ---------------------------------------------------------------------------

kernel void FindFieldEdges(
	const int4 offset,
	global int* materials,
	global int* edgeOccupancy,
	global int* edgeIndices)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);
	const int z = get_global_id(2);
	
	const int4 pos = { x, y, z, 0 };
	const int index = (x + (y * HERMITE_INDEX_SIZE) + (z * HERMITE_INDEX_SIZE * HERMITE_INDEX_SIZE));
	const int edgeIndex = index * 3;

	const int CORNER_MATERIALS[4] = 
	{
		materials[field_index(pos + (int4)(0, 0, 0, 0))],
		materials[field_index(pos + (int4)(1, 0, 0, 0))],
		materials[field_index(pos + (int4)(0, 1, 0, 0))],
		materials[field_index(pos + (int4)(0, 0, 1, 0))],
	};

	const int voxelIndex = pos.x | (pos.y << VOXEL_INDEX_SHIFT) | (pos.z << (VOXEL_INDEX_SHIFT * 2));

#pragma unroll
	for (int i = 0; i < 3; i++)
	{
		const int e = 1 + i;
		const int signChange = 
				((CORNER_MATERIALS[0] != MATERIAL_AIR && CORNER_MATERIALS[e] == MATERIAL_AIR) || 
				(CORNER_MATERIALS[0] == MATERIAL_AIR && CORNER_MATERIALS[e] != MATERIAL_AIR)) ? 1 : 0;

		edgeOccupancy[edgeIndex + i] = signChange;
		edgeIndices[edgeIndex + i] = signChange ? ((voxelIndex << 2) | i) : -1;
	}
}

// ---------------------------------------------------------------------------

kernel void CompactEdges(
	global int* edgeValid,
	global int* edgeScan,
	global int* edges,
	global int* compactActiveEdges)
{
	const int index = get_global_id(0);

	if (edgeValid[index])
	{
		compactActiveEdges[edgeScan[index]] = edges[index];
	}
}

// ---------------------------------------------------------------------------

kernel void FindEdgeIntersectionInfo(
	read_only image2d_t permTexture,
	const int4 worldSpaceOffset,
	const int sampleScale,
	global int* encodedEdges,
	global float4* edgeInfo)
{
	const int index = get_global_id(0);
	const int edge = encodedEdges[index];

	const int axisIndex = edge & 3;
	const int hermiteIndex = edge >> 2;

	const int4 local_pos =
	{
		(hermiteIndex >> (VOXEL_INDEX_SHIFT * 0)) & VOXEL_INDEX_MASK,
		(hermiteIndex >> (VOXEL_INDEX_SHIFT * 1)) & VOXEL_INDEX_MASK,
		(hermiteIndex >> (VOXEL_INDEX_SHIFT * 2)) & VOXEL_INDEX_MASK,
		0
	};

	const int4 world_pos = (sampleScale * local_pos) + worldSpaceOffset;
	const float4 p0 = convert_float4(world_pos);
	const float4 p1 = convert_float4(world_pos + (sampleScale * EDGE_END_OFFSETS[axisIndex]));
	const float4 offset = convert_float4(worldSpaceOffset);

	float minValue = FLT_MAX;
	float currentT = 0.f;
	float t = 0.f;
	for (int i = 0; i <= FIND_EDGE_INFO_STEPS; i++)
	{
		const float4 p = mix(p0, p1, currentT);
		const float	d = fabs(DensityFunc(p, permTexture));
		if (d < minValue)
		{
			t = currentT;
			minValue = d;
		}

		currentT += FIND_EDGE_INFO_INCREMENT;
	}
	
	const float4 p = mix(p0, p1, t);

	const float h = 0.001f;
	const float4 xOffset = { h, 0, 0, 0 };
	const float4 yOffset = { 0, h, 0, 0 };
	const float4 zOffset = { 0, 0, h, 0 };

	const float dx = DensityFunc(p + xOffset, permTexture) - DensityFunc(p - xOffset, permTexture);
	const float dy = DensityFunc(p + yOffset, permTexture) - DensityFunc(p - yOffset, permTexture);
	const float dz = DensityFunc(p + zOffset, permTexture) - DensityFunc(p - zOffset, permTexture);

	const float3 normal = normalize((float3)(dx, dy, dz));
	edgeInfo[index] = (float4)(normal, t);
} 

// ---------------------------------------------------------------------------
