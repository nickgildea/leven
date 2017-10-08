// ---------------------------------------------------------------------------

#include "cl/hg_sdf.glsl"

typedef struct CSGOperation_s
{
	int			type;
	int			brushFunction;
	int			brushMaterial;
	float		rotateY;
	float4		origin;
	float4		dimensions;
}
CSGOperation;

// ---------------------------------------------------------------------------

float4 RotateX(const float4 v, const float angle)
{
	float4 result = v;
	const float s = sin(radians(angle));
	const float c = cos(radians(angle));
	result.y =  v.y * c + -v.z * s;
	result.z =  v.y * s +  v.z * c;

	return result;
}

float4 RotateY(const float4 v, const float angle)
{
	float4 result = v;
	const float s = sin(radians(angle));
	const float c = cos(radians(angle));
	result.x =  v.x * c +  v.z * s;
	result.z = -v.x * s +  v.z * c;

	return result;
}

// ---------------------------------------------------------------------------

float Density_Sphere(const float4 world_pos, const float4 origin, const float radius)
{
	return length((world_pos - origin).xyz) - radius;
}

// ---------------------------------------------------------------------------

float Density_Cuboid(
	const float4 world_pos, 
	const float4 origin, 
	const float4 dimensions,
	const float rotateY)
{
	const float4 local_pos = world_pos - origin;
#if 1
	float3 p = local_pos.xyz;
	float2 pxz = p.xz;
	pR(&pxz, rotateY);
	p.xz = pxz;
	return fBox(p, dimensions.xyz); 
#else
	const float4 pos = RotateY(local_pos, rotateY);
	const float4 d = fabs(pos) - dimensions;
	const float m = max(d.x, max(d.y, d.z));
	return min(m, length(max(d, (float4)(0.f))));
#endif
}

// ---------------------------------------------------------------------------

float BrushDensity(
	const float4 worldspaceOffset, 
	const CSGOperation* op)
{
	float brushDensity[NUM_CSG_BRUSHES] = { FLT_MAX, FLT_MAX };
	brushDensity[0] = Density_Cuboid(worldspaceOffset, op->origin, op->dimensions, op->rotateY);
	brushDensity[1] = Density_Sphere(worldspaceOffset, op->origin, op->dimensions.x);

	return brushDensity[op->brushFunction];
}

// ---------------------------------------------------------------------------

// "concatenate" the brush operations to get the final density for the brush in isolation
float BrushZeroCrossing(
	const float4 p0, 
	const float4 p1, 
	const int numOperations,
	global const CSGOperation* operations)
{
	float minDensity = FLT_MAX;
	float crossing = 0.f;
	for (float t = 0.f; t <= 1.f; t += (1.f/16.f))
	{
		const float4 p = mix(p0, p1, t);
		for (int i = 0; i < numOperations; i++)
		{
			const CSGOperation op = operations[i];
			const float d = fabs(BrushDensity(p, &op));
			if (d < minDensity)
			{
				crossing = t;
				minDensity = d;
			}
		}
	}

	return crossing;
}

// ---------------------------------------------------------------------------

int BrushMaterial(
	const float4 world_pos, 
	const int numOperations,
	global const CSGOperation* operations,
	const int material)
{
	int m = material;

	for (int i = 0; i < numOperations; i++)
	{
		const CSGOperation op = operations[i];

		const int operationMaterial[2] =
		{
			op.brushMaterial,
			MATERIAL_AIR,
		};

		const float d = BrushDensity(world_pos, &op);
		if (d <= 0.f)
		{
			m = operationMaterial[op.type];
		}
	}

	return m;
}

// ---------------------------------------------------------------------------

float3 BrushNormal(
	const float4 world_pos, 
	const int numOperations,
	global const CSGOperation* operations)
{
	float3 normal = { 0.f, 0.f, 0.f };
	for (int i = 0; i < numOperations; i++)
	{
		const CSGOperation op = operations[i];
		const float d = BrushDensity(world_pos, &op);
		if (d > 0.f)
		{
		//	 flip = operationType[i] == 0 ? 1.f : -1.f;
			continue;
		}

		const float h = 0.001f;
		const float dx0 = BrushDensity(world_pos + (float4)(h, 0, 0, 0), &op);
		const float dx1 = BrushDensity(world_pos - (float4)(h, 0, 0, 0), &op);

		const float dy0 = BrushDensity(world_pos + (float4)(0, h, 0, 0), &op);
		const float dy1 = BrushDensity(world_pos - (float4)(0, h, 0, 0), &op);
		
		const float dz0 = BrushDensity(world_pos + (float4)(0, 0, h, 0), &op);
		const float dz1 = BrushDensity(world_pos - (float4)(0, 0, h, 0), &op);

		const float flip = op.type == 0 ? 1.f : -1.f;
		normal = flip * normalize((float3)(dx0 - dx1, dy0 - dy1, dz0 - dz1));
	}

	return normal;
}

// ---------------------------------------------------------------------------

kernel
void CSG_HermiteIndices(
	const int4 worldspaceOffset,
	const int numOperations,
	global const CSGOperation* operations,
	const int sampleScale,
	global const int* field_materials,
	global int* updated_indices,
	global int4* updated_positions,
	global int* updatedMaterials)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);
	const int z = get_global_id(2);
	const int4 local_pos = { x, y, z, 0 };

	const int sx = sampleScale * x;
	const int sy = sampleScale * y;
	const int sz = sampleScale * z;

	const int index = field_index(local_pos);
	const int oldMaterial = field_materials[index];
	int material = field_materials[index];

	const float4 world_pos = { worldspaceOffset.x + sx, worldspaceOffset.y + sy, worldspaceOffset.z + sz, 0 };
	material = BrushMaterial(world_pos, numOperations, operations, material);

	const int updated = material != oldMaterial;
	updated_indices[index] = updated;
	updated_positions[index] = local_pos;
	updatedMaterials[index] = material;
}

// ---------------------------------------------------------------------------

kernel void CompactPoints(
	global int* updated_indices,
	global int4* points,
	global const int* operationMaterials,
	global int* scan,
	global int4* compact_points,
	global int* compactOpMaterials)
{
	const int index = get_global_id(0);
	if (updated_indices[index])
	{
		compact_points[scan[index]] = points[index];
		compactOpMaterials[scan[index]] = operationMaterials[index];
	}
}

// ---------------------------------------------------------------------------

kernel void UpdateFieldMaterials(
	global const int4* fieldPositions,
	global const int* operationMaterials,
	global int* fieldMaterials)
{
	const int id = get_global_id(0);
	const int4 fieldPos = fieldPositions[id];
	const int fieldIndex = field_index(fieldPos);
	fieldMaterials[fieldIndex] = operationMaterials[id];
}


// ---------------------------------------------------------------------------

__constant int4 EDGE_OFFSETS[3] =
{
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
};

kernel void FindUpdatedEdges(
	global int4* updatedHermiteIndices,
	global int* updatedHermiteEdgeIndices
	)
{
	const int id = get_global_id(0);
	const int edgeIndex = id * 6;
	
	const int4 pos = updatedHermiteIndices[id];
	const int posIndex = (pos.x | (pos.y << VOXEL_INDEX_SHIFT) | (pos.z << (VOXEL_INDEX_SHIFT * 2))) << 2;

	updatedHermiteEdgeIndices[edgeIndex + 0] = posIndex | 0;
	updatedHermiteEdgeIndices[edgeIndex + 1] = posIndex | 1;
	updatedHermiteEdgeIndices[edgeIndex + 2] = posIndex | 2;

#if 0
	// TODO this is illegal due to the pos[i] indexing, but would be a nicer impl if I can workaround that
	for (int i = 0; i < 3; i++)
	{
		if (pos[i] > 0)
		{
			const int4 edgePos = pos - EDGE_OFFSETS[i];
			int edgeIndex = (
				(edgePos.x << (0 * VOXEL_INDEX_SHIFT)) | 
				(edgePos.y << (1 * VOXEL_INDEX_SHIFT)) | 
				(edgePos.z << (2 * VOXEL_INDEX_SHIFT))
			);
			
			updatedHermiteEdgeIndices[edgeIndex + 3 + i] = (edgeIndex << 2) | i;
		}
		else
		{
			updatedHermiteEdgeIndices[edgeIndex + 3 + i] = -1;
		}
	}

#else

	if (pos.x > 0)
	{
		const int4 xPos = pos - (int4)(1, 0, 0, 0);
		const int xPosIndex = (xPos.x | (xPos.y << VOXEL_INDEX_SHIFT) | (xPos.z << (VOXEL_INDEX_SHIFT * 2))) << 2;
		updatedHermiteEdgeIndices[edgeIndex + 3] = xPosIndex | 0;
	}
	else
	{
		updatedHermiteEdgeIndices[edgeIndex + 3] = -1;
	}

	if (pos.y > 0)
	{
		const int4 yPos = pos - (int4)(0, 1, 0, 0);
		const int yPosIndex = (yPos.x | (yPos.y << VOXEL_INDEX_SHIFT) | (yPos.z << (VOXEL_INDEX_SHIFT * 2))) << 2;
		updatedHermiteEdgeIndices[edgeIndex + 4] = yPosIndex | 1;
	}
	else
	{
		updatedHermiteEdgeIndices[edgeIndex + 4] = -1;
	}

	if (pos.z > 0)
	{
		const int4 zPos = pos - (int4)(0, 0, 1, 0);
		const int zPosIndex = (zPos.x | (zPos.y << VOXEL_INDEX_SHIFT) | (zPos.z << (VOXEL_INDEX_SHIFT * 2))) << 2;
		updatedHermiteEdgeIndices[edgeIndex + 5] = zPosIndex | 2;
	}
	else
	{
		updatedHermiteEdgeIndices[edgeIndex + 5] = -1;
	}
#endif
}

// ---------------------------------------------------------------------------

kernel void RemoveInvalidIndices(
	global int* generatedEdgeIndices,
	global int* edgeValid
	)
{
	int id = get_global_id(0);
	edgeValid[id] = generatedEdgeIndices[id] != -1;
}

// ---------------------------------------------------------------------------

kernel void FilterValidEdges(
	global int* generatedHermiteEdgeIndices,
	global int* materials,
	global int* edgeValid
	)
{
	const int id = get_global_id(0);
	const int generatedEdgeIndex = generatedHermiteEdgeIndices[id];

	const int edgeNumber = generatedEdgeIndex & 3;
	const int edgeIndex = generatedEdgeIndex >> 2;
	const int4 position = 
	{
		(edgeIndex >> (VOXEL_INDEX_SHIFT * 0)) & VOXEL_INDEX_MASK,
		(edgeIndex >> (VOXEL_INDEX_SHIFT * 1)) & VOXEL_INDEX_MASK,
		(edgeIndex >> (VOXEL_INDEX_SHIFT * 2)) & VOXEL_INDEX_MASK,
		0
	};	

	const int index0 = field_index(position);
	const int index1 = field_index(position + EDGE_OFFSETS[edgeNumber]);

	// There should be no need to check these indices, the previous call to 
	// RemoveInvalidIndices should have validated the generatedEdgeIndices array
#ifdef PARANOID
	const int material0 = index0 < FIELD_BUFFER_SIZE ? materials[index0] : MATERIAL_AIR;
	const int material1 = index1 < FIELD_BUFFER_SIZE ? materials[index1] : MATERIAL_AIR;
#else
	const int material0 = materials[index0];
	const int material1 = materials[index1];
#endif

	const int signChange = (material0 == MATERIAL_AIR && material1 != MATERIAL_AIR) ||
		(material1 == MATERIAL_AIR && material0 != MATERIAL_AIR);

	edgeValid[id] = signChange && generatedEdgeIndex != -1;
}

// ---------------------------------------------------------------------------

kernel void SelectValidFieldEdges(
	global int* fieldEdgeIndices,
	global int* invalidatedEdgesIndices,
	const int numInvalidatedEdges,
	global int* fieldEdgeValid
)
{
	const int id = get_global_id(0);
	const int edgeIndex = fieldEdgeIndices[id];

	int invalidated = 0;
	for (int i = 0; i < numInvalidatedEdges; i++)
	{
		invalidated += (invalidatedEdgesIndices[i] == edgeIndex);
	}

	fieldEdgeValid[id] = invalidated == 0;
}

// ---------------------------------------------------------------------------

kernel void PruneFieldEdges(
	global int* fieldEdgeIndices,
	global int* invalidatedEdgeIndices,
	const int numInvalidatedEdges,
	global int* fieldEdgeValidity
	)
{
	const int id = get_global_id(0);
	const int edgeIndex = fieldEdgeIndices[id];

	int invalid = 0;
	for (int i = 0; i < numInvalidatedEdges; i++)
	{
		invalid |= (invalidatedEdgeIndices[i] == edgeIndex);
	}

	fieldEdgeValidity[id] = invalid ? 0 : 1;
}

// ---------------------------------------------------------------------------

kernel void CompactFieldEdges(
	global int* edgeValid,
	global int* edgeScan,
	global int* edgeIndices,
	global float4* edgeNormals,
	global int* compactIndices,
	global float4* compactNormals
	)
{
	const int index = get_global_id(0);
	if (edgeValid[index])
	{
		const int cid = edgeScan[index];	
		compactIndices[cid] = edgeIndices[index];
		compactNormals[cid] = edgeNormals[index];
	}
}

// ---------------------------------------------------------------------------

// TODO this is almost identical to the FindEdgeIntersectionInfo in density_field.cl except it uses 
// the BrushDensity func rather than DensityFunc
kernel void FindEdgeIntersectionInfo(
	const int4 offset,
	const int numOperations,
	global const CSGOperation* operations,
	const int sampleScale,
	global const int* compactEdges,
	global float4* normals)
{
	const int index = get_global_id(0);
	const int globalEdgeIndex = compactEdges[index];

	const int edgeIndex = 4 * (globalEdgeIndex & 3);
	const int voxelIndex = globalEdgeIndex >> 2;

	const int4 local_pos =
	{
		(voxelIndex >> (VOXEL_INDEX_SHIFT * 0)) & VOXEL_INDEX_MASK,
		(voxelIndex >> (VOXEL_INDEX_SHIFT * 1)) & VOXEL_INDEX_MASK,
		(voxelIndex >> (VOXEL_INDEX_SHIFT * 2)) & VOXEL_INDEX_MASK,
		0
	};

	const int e0 = EDGE_MAP[edgeIndex][0];
	const int e1 = EDGE_MAP[edgeIndex][1];

	const int4 world_pos = (sampleScale * local_pos) + offset;
	const float4 p0 = convert_float4(world_pos + CHILD_MIN_OFFSETS[e0]);
	const float4 p1 = convert_float4(world_pos + (sampleScale * CHILD_MIN_OFFSETS[e1]));

	const float t = BrushZeroCrossing(p0, p1, numOperations, operations);
	const float4 p = mix(p0, p1, t);

	const float3 n = BrushNormal(p, numOperations, operations);
	normals[index] = (float4)(n, t);
} 


