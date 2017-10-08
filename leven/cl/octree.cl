// ---------------------------------------------------------------------------

int4 DecodeVoxelIndex(uint index)
{
	int4 p = { 0, 0, 0, 0 };
	p.x = (index >> (VOXEL_INDEX_SHIFT * 0)) & VOXEL_INDEX_MASK;
	p.y = (index >> (VOXEL_INDEX_SHIFT * 1)) & VOXEL_INDEX_MASK;
	p.z = (index >> (VOXEL_INDEX_SHIFT * 2)) & VOXEL_INDEX_MASK;

	return p;
}
//
// ---------------------------------------------------------------------------

uint EncodeVoxelIndex(int4 pos)
{
	uint encoded = 0;
	encoded |= pos.x << (VOXEL_INDEX_SHIFT * 0);
	encoded |= pos.y << (VOXEL_INDEX_SHIFT * 1);
	encoded |= pos.z << (VOXEL_INDEX_SHIFT * 2);

	return encoded;
}

// ---------------------------------------------------------------------------

inline int MSB(uint n)
{
	return 32 - clz(n);
}

// ---------------------------------------------------------------------------

uint CodeForPosition(int4 p, int nodeDepth)
{
	uint code = 1;
	for (int depth = MAX_OCTREE_DEPTH - 1; depth >= (MAX_OCTREE_DEPTH - nodeDepth); depth--)
	{
		int x = (p.x >> depth) & 1;
		int y = (p.y >> depth) & 1;
		int z = (p.z >> depth) & 1;
		int c = (x << 2) | (y << 1) | z;
		code = (code << 3) | c;
	}

	return code;
}

// ---------------------------------------------------------------------------

int4 PositionForCode(uint code)
{
	const int msb = MSB(code);
	const int nodeDepth = (msb / 3);

	int4 pos = { 0, 0, 0, 0 };
	for (int i = MAX_OCTREE_DEPTH - nodeDepth; i < MAX_OCTREE_DEPTH; i++)
	{
		uint c = code & 7;
		code >>= 3;

		int x = (c >> 2) & 1;
		int y = (c >> 1) & 1;
		int z = (c >> 0) & 1;

		pos.x |= (x << i);
		pos.y |= (y << i);
		pos.z |= (z << i);
	}

	return pos;
}

// ---------------------------------------------------------------------------

// TODO sort network?
// see http://stackoverflow.com/questions/2786899/fastest-sort-of-fixed-length-6-int-array
void InlineInsertionSwap8(int* data)
{
	int i, j;

#pragma unroll
	for (i = 1; i < 8; i++) 
	{
		int tmp = data[i];
		for (j = i; j >= 1 && tmp < data[j-1]; j--)
		{
			data[j] = data[j-1];
		}
		data[j] = tmp;
	}
}

// ---------------------------------------------------------------------------

int FindDominantMaterial(const int m[8])
{
	int data[8] = { m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7] };
	InlineInsertionSwap8(data);

	int current = data[0];
	int count = 1;
	int maxCount = 0;
	int maxMaterial = 0;

#pragma unroll
	for (int i = 1; i < 8; i++)
	{
		int m = data[i];
		if (m == MATERIAL_AIR || m == MATERIAL_NONE)
		{
			continue;
		}

		if (current != m)
		{
			if (count > maxCount)
			{
				maxCount = count;
				maxMaterial = current;
			}

			current = m;
			count = 1;
		}
		else
		{
			count++;
		}
	}

	if (count > maxCount)
	{
		maxMaterial = current;
	}

	return maxMaterial;
}

// ---------------------------------------------------------------------------

kernel void FindActiveVoxels(
	global int* materials,
	global int* voxelOccupancy,
	global int* voxelEdgeInfo,
	global int* voxelPositions,
	global int* voxelMaterials)
{
	const int x = get_global_id(0);
	const int y = get_global_id(1);
	const int z = get_global_id(2);
	
	const int index = x + (y * VOXELS_PER_CHUNK) + (z * VOXELS_PER_CHUNK * VOXELS_PER_CHUNK);
	const int4 pos = { x, y, z, 0 };

	const int cornerMaterials[8] = 
	{
		materials[field_index(pos + CHILD_MIN_OFFSETS[0])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[1])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[2])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[3])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[4])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[5])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[6])],
		materials[field_index(pos + CHILD_MIN_OFFSETS[7])],
	};

	// record the on/off values at the corner of each voxel
	int cornerValues = 0;
	cornerValues |= (((cornerMaterials[0]) == MATERIAL_AIR ? 0 : 1) << 0);
	cornerValues |= (((cornerMaterials[1]) == MATERIAL_AIR ? 0 : 1) << 1);
	cornerValues |= (((cornerMaterials[2]) == MATERIAL_AIR ? 0 : 1) << 2);
	cornerValues |= (((cornerMaterials[3]) == MATERIAL_AIR ? 0 : 1) << 3);
	cornerValues |= (((cornerMaterials[4]) == MATERIAL_AIR ? 0 : 1) << 4);
	cornerValues |= (((cornerMaterials[5]) == MATERIAL_AIR ? 0 : 1) << 5);
	cornerValues |= (((cornerMaterials[6]) == MATERIAL_AIR ? 0 : 1) << 6);
	cornerValues |= (((cornerMaterials[7]) == MATERIAL_AIR ? 0 : 1) << 7);

	// record which of the 12 voxel edges are on/off
	int edgeList = 0;

#pragma unroll
	for (int i = 0; i < 12; i++)
	{
		const int i0 = EDGE_MAP[i][0];	
		const int i1 = EDGE_MAP[i][1];	
		const int edgeStart = (cornerValues >> i0) & 1;
		const int edgeEnd = (cornerValues >> i1) & 1;

		const int signChange = (!edgeStart && edgeEnd) || (edgeStart && !edgeEnd);
		edgeList |= (signChange << i);
	}

	voxelOccupancy[index] = cornerValues != 0 && cornerValues != 255;
	voxelPositions[index] = CodeForPosition(pos, MAX_OCTREE_DEPTH);
	voxelEdgeInfo[index] = edgeList;

	// store cornerValues here too as its needed by the CPU side and edgeInfo isn't exported
	int materialIndex = FindDominantMaterial(cornerMaterials);
	voxelMaterials[index] = (materialIndex << 8) | cornerValues;
}

// ---------------------------------------------------------------------------

kernel void CompactVoxels(
	global int* voxelValid,
	global int* voxelEdgeInfo,
	global int* voxelPositions,
	global int* voxelMaterials,
	global int* voxelScan,
	global int* compactPositions,
	global int* compactEdgeInfo,
	global int* compactMaterials)
{
	const int index = get_global_id(0);

	if (voxelValid[index])
	{
		compactPositions[voxelScan[index]] = voxelPositions[index];
		compactEdgeInfo[voxelScan[index]] = voxelEdgeInfo[index];
		compactMaterials[voxelScan[index]] = voxelMaterials[index];
	}
}

// ---------------------------------------------------------------------------

constant int EDGE_VERTEX_MAP[12][2] = 
{
	{0,4},{1,5},{2,6},{3,7},	// x-axis 
	{0,2},{1,3},{4,6},{5,7},	// y-axis
	{0,1},{2,3},{4,5},{6,7}		// z-axis
};

// ---------------------------------------------------------------------------

kernel void CreateLeafNodes(
	const int sampleScale,
	global int* voxelPositions,
	global int* voxelEdgeInfo,
	global float4* edgeDataTable,
	global float4* vertexNormals,
	global QEFData* leafQEFs,
	global ulong* cuckoo_table,
	global ulong* cuckoo_stash,
	const uint   cuckoo_prime,
	global uint* cuckoo_hashParams,
	const int    cuckoo_checkStash)
{
	const int index = get_global_id(0);

	const int encodedPosition = voxelPositions[index];
	const int4 position = PositionForCode(encodedPosition);
	const float4 position_f = convert_float4(position);

	const int edgeInfo = voxelEdgeInfo[index];
	const int edgeList = edgeInfo;

	float4 edgePositions[12], edgeNormals[12];
	int edgeCount = 0;

#pragma unroll
	for (int i = 0; i < 12; i++)
	{
		const int active = (edgeList >> i) & 1;
		if (!active)
		{
			continue;
		}

		const int e0 = EDGE_VERTEX_MAP[i][0];
		const int e1 = EDGE_VERTEX_MAP[i][1];

		const float4 p0 = position_f + (convert_float4(CHILD_MIN_OFFSETS[e0]));
		const float4 p1 = position_f + (convert_float4(CHILD_MIN_OFFSETS[e1]));
	
		// this works due to the layout EDGE_VERTEX_MAP, the first 4 elements are the X axis
		// the next 4 are the Y axis and the last 4 are the Z axis
		const int axis = i / 4;

		const int4 hermiteIndexPosition = position + CHILD_MIN_OFFSETS[e0];
		const int edgeIndex = (EncodeVoxelIndex(hermiteIndexPosition) << 2) | axis;

		const uint dataIndex = Cuckoo_Find(edgeIndex,
			cuckoo_table, cuckoo_stash, cuckoo_prime,
			cuckoo_hashParams, cuckoo_checkStash);

		if (dataIndex != ~0U)
		{
			const float4 edgeData = edgeDataTable[dataIndex];
			edgePositions[edgeCount] = sampleScale * mix(p0, p1, edgeData.w);
			edgeNormals[edgeCount] = (float4)(edgeData.x, edgeData.y, edgeData.z, 0.f);

			edgeCount++;
		}
	}

	QEFData qef;
	qef_create_from_points(edgePositions, edgeNormals, edgeCount, &qef);
	leafQEFs[index] = qef;

	float4 normal = { 0.f, 0.f, 0.f, 0.f };
	for (int i = 0; i < edgeCount; i++)
	{
		normal += edgeNormals[i];
		normal.w += 1.f;
	}
	
	normal /= normal.w;
	normal.w = 0.f;

	vertexNormals[index] = normal;
}

// ---------------------------------------------------------------------------

kernel void SolveQEFs(
	const float4 worldSpaceOffset,
	global QEFData* qefs,
	global float4* solvedPositions)
{
	const int index = get_global_id(0);

	QEFData qef = qefs[index];
	float4 pos = { 0.f, 0.f, 0.f, 0.f };
	qef_solve(&qef, &pos);

	pos = (pos * LEAF_SIZE_SCALE) + worldSpaceOffset;
	pos.w = 1.f;

	solvedPositions[index] = pos;
}

// ---------------------------------------------------------------------------

int ProcessEdge(
	const int* nodeIndices,
	const int nodeMaterial,
	const int axis,
	global int* indexBuffer)
{
	const int edge = (axis * 4) + 3;
	const int c1 = EDGE_VERTEX_MAP[edge][0];
	const int c2 = EDGE_VERTEX_MAP[edge][1];

	const int corners = nodeMaterial & 0xff;
	const int m1 = (corners >> c1) & 1;
	const int m2 = (corners >> c2) & 1;

	const int signChange = (m1 && !m2) || (!m1 && m2);
	if (!signChange)
	{
		return 0;
	}

	// flip the winding depending on which end of the edge is outside the volume
	const int flip = m1 != 0 ? 1 : 0;
	const uint indices[2][6] = 
	{
		// different winding orders depending on the sign change direction
		{ 0, 1, 3, 0, 3, 2 },
		{ 0, 3, 1, 0, 2, 3 },
	};

	indexBuffer[0] = nodeIndices[indices[flip][0]];
	indexBuffer[1] = nodeIndices[indices[flip][1]];
	indexBuffer[2] = nodeIndices[indices[flip][2]];
	indexBuffer[3] = nodeIndices[indices[flip][3]];
	indexBuffer[4] = nodeIndices[indices[flip][4]];
	indexBuffer[5] = nodeIndices[indices[flip][5]];

	return 1;
}

// ---------------------------------------------------------------------------

constant int4 EDGE_NODE_OFFSETS[3][4] =
{
	{ (int4)(0, 0, 0, 0), (int4)(0, 0, 1, 0), (int4)(0, 1, 0, 0), (int4)(0, 1, 1, 0) },
	{ (int4)(0, 0, 0, 0), (int4)(1, 0, 0, 0), (int4)(0, 0, 1, 0), (int4)(1, 0, 1, 0) },
	{ (int4)(0, 0, 0, 0), (int4)(0, 1, 0, 0), (int4)(1, 0, 0, 0), (int4)(1, 1, 0, 0) },
};

// ---------------------------------------------------------------------------

kernel void GenerateMesh(
	global uint* octreeNodeCodes,
	global int* octreeMaterials,
	global int* meshIndexBuffer,
	global int* trianglesValid,
	global ulong* cuckoo_table,
	global ulong* cuckoo_stash,
	const uint   cuckoo_prime,
	global uint* cuckoo_hashParams,
	const int    cuckoo_checkStash)
{
	const int index = get_global_id(0);
	const uint code = octreeNodeCodes[index];
	const int triIndex = index * 3;
	
	const int4 offset = PositionForCode(code);
	const int pos[3] = { offset.x, offset.y, offset.z };

	int nodeIndices[4] = { ~0, ~0, ~0, ~0 };
	for (int axis = 0; axis < 3; axis++)
	{
		trianglesValid[triIndex + axis] = 0;

		// need to check that the position generated when the offsets are added won't exceed 
		// the chunk bounds -- if this happens rather than failing the octree lookup 
		// will actually wrap around to 0 again causing weird polys to be generated
		const int a = pos[(axis + 1) % 3];
		const int b = pos[(axis + 2) % 3];
		const int isEdgeVoxel = a == (VOXELS_PER_CHUNK - 1) || b == (VOXELS_PER_CHUNK - 1);
		if (isEdgeVoxel)
		{
			continue;
		}

		nodeIndices[0] = index;
	
#pragma unroll
		for (int n = 1; n < 4; n++)
		{
			const int4 p = offset + EDGE_NODE_OFFSETS[axis][n];
			const uint c = CodeForPosition(p, MAX_OCTREE_DEPTH);

			nodeIndices[n] = Cuckoo_Find(c,
				cuckoo_table, cuckoo_stash, cuckoo_prime,
				cuckoo_hashParams, cuckoo_checkStash);
		}

		if (nodeIndices[1] != ~0 &&
			nodeIndices[2] != ~0 &&
			nodeIndices[3] != ~0)
		{
			const int bufferOffset = (triIndex * 6) + (axis * 6);
			const int trisEmitted = 
				ProcessEdge(&nodeIndices[0], octreeMaterials[index], axis, &meshIndexBuffer[bufferOffset]);
			trianglesValid[triIndex + axis] = trisEmitted;
		}
	}
}

// ---------------------------------------------------------------------------

kernel void CompactMeshTriangles(
	global int* trianglesValid,
	global int* trianglesScan,
	global int* meshIndexBuffer,
	global int* compactMeshIndexBuffer)
{
	const int index = get_global_id(0);
	if (trianglesValid[index])
	{
		const int scanOffset = trianglesScan[index] * 6;
		const int bufferOffset = (index * 6);

#pragma unroll
		for (int i = 0; i < 6; i++)
		{
			compactMeshIndexBuffer[scanOffset + i] = meshIndexBuffer[bufferOffset + i];
		}
	}
}

// ---------------------------------------------------------------------------

struct MeshVertex
{
	float4		position, normal, colour;
};

// ---------------------------------------------------------------------------

kernel void GenerateMeshVertexBuffer(
	global float4* vertexPositions,
	global float4* vertexNormals,
	global int* nodeMaterials,
	const float4 colour,
	global struct MeshVertex* meshVertexBuffer)
{
	const int index = get_global_id(0);
	const int material = nodeMaterials[index];
	meshVertexBuffer[index].position = vertexPositions[index];
	meshVertexBuffer[index].normal = vertexNormals[index];
	meshVertexBuffer[index].colour = (float4)(colour.x, colour.y, colour.z, (float)(material >> 8));
}

// ---------------------------------------------------------------------------

kernel void UpdateNodeCodes(
	const int4 chunkOffset,
	const int selectedNodeSize,
	global uint* nodeCodes)
{
	const int index = get_global_id(0);
	const int code = nodeCodes[index];
	int4 position = PositionForCode(code);
	position /= selectedNodeSize;
	position += chunkOffset;
	nodeCodes[index] = CodeForPosition(position, MAX_OCTREE_DEPTH);
}

// ---------------------------------------------------------------------------

kernel void FindSeamNodes(
	global uint* nodeCodes,
	global int* isSeamNode)
{
	const int index = get_global_id(0);
	const uint code = nodeCodes[index];
	
	int4 position = PositionForCode(code);
	int xSeam = position.x == 0 || position.x == (VOXELS_PER_CHUNK - 1);
	int ySeam = position.y == 0 || position.y == (VOXELS_PER_CHUNK - 1);
	int zSeam = position.z == 0 || position.z == (VOXELS_PER_CHUNK - 1);
	isSeamNode[index] = xSeam | ySeam | zSeam;
}

// ---------------------------------------------------------------------------

typedef struct SeamNodeInfo_s
{
	int4			localspaceMin;
	float4			position;
	float4			normal;
} SeamNodeInfo;

kernel void ExtractSeamNodeInfo(
	global int* isSeamNode,
	global int* isSeamNodeScan,
	global uint* octreeCodes,
	global int* octreeMaterials,
	global float4* octreePositions,
	global float4* octreeNormals,
	global SeamNodeInfo* seamNodeInfo)
{
	const int index = get_global_id(0);
	if (isSeamNode[index])
	{
		int scan = isSeamNodeScan[index];

		SeamNodeInfo info;
		info.localspaceMin = PositionForCode(octreeCodes[index]);
		info.position = octreePositions[index];
		info.normal = octreeNormals[index];
		info.localspaceMin.w = octreeMaterials[index];

		seamNodeInfo[scan] = info;
	}
}


// ---------------------------------------------------------------------------




