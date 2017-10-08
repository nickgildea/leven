#include	"compute_local.h"
#include	"compute_program.h"
#include	"volume_constants.h"
#include	"volume_materials.h"

#include	<Remotery.h>
#include	<sstream>

// ----------------------------------------------------------------------------

int ApplyCSGOperations(
	MeshGenerationContext* meshGen,
	const std::vector<CSGOperationInfo>& opInfo,
	const glm::ivec3& clipmapNodeMin,
	const int clipmapNodeSize,
	GPUDensityField& field)
{
	rmt_ScopedCPUSample(ApplyCSGOperations);

	if (opInfo.empty())
	{
		return CL_SUCCESS;
	}

	const cl_int4 fieldOffset = LeafScaleVec(clipmapNodeMin);
	const int sampleScale = clipmapNodeSize / (LEAF_SIZE_SCALE * meshGen->voxelsPerChunk);

	std::vector<CSGOperationInfo> fuckSake = opInfo;
	cl::Buffer d_operations;
	CL_CALL(CreateBuffer(CL_MEM_READ_ONLY, sizeof(CSGOperationInfo) * opInfo.size(), &fuckSake[0], d_operations));

	auto ctx = GetComputeContext();
	int index = 0;

	u32 numUpdatedPoints = 0;
	cl::Buffer d_compactUpdatedPoints, d_compactUpdatedMaterials;
	{
		rmt_ScopedCPUSample(Apply);

		const int fieldBufferSize = meshGen->fieldSize * meshGen->fieldSize * meshGen->fieldSize; 
		cl::Buffer d_updatedIndices(ctx->context, CL_MEM_READ_WRITE, fieldBufferSize * sizeof(int));
		cl::Buffer d_updatedPoints(ctx->context, CL_MEM_READ_WRITE, fieldBufferSize * sizeof(glm::ivec4));
		cl::Buffer d_updatedMaterials(ctx->context, CL_MEM_READ_WRITE, fieldBufferSize * sizeof(int));

		index = 0;
		cl::Kernel k_applyCSGOp(meshGen->csgProgram.get(), "CSG_HermiteIndices");
		CL_CALL(k_applyCSGOp.setArg(index++, fieldOffset));
		CL_CALL(k_applyCSGOp.setArg(index++, (u32)opInfo.size()));
		CL_CALL(k_applyCSGOp.setArg(index++, d_operations));
		CL_CALL(k_applyCSGOp.setArg(index++, sampleScale));
		CL_CALL(k_applyCSGOp.setArg(index++, field.materials));
		CL_CALL(k_applyCSGOp.setArg(index++, d_updatedIndices));
		CL_CALL(k_applyCSGOp.setArg(index++, d_updatedPoints));
		CL_CALL(k_applyCSGOp.setArg(index++, d_updatedMaterials));

		const cl::NDRange applyCSGSize(meshGen->fieldSize, meshGen->fieldSize, meshGen->fieldSize);
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_applyCSGOp, cl::NullRange, applyCSGSize, cl::NullRange));

		cl::Buffer d_updatedIndicesScan(ctx->context, CL_MEM_READ_WRITE, fieldBufferSize * sizeof(int));
		numUpdatedPoints = ExclusiveScan(ctx->queue, d_updatedIndices, d_updatedIndicesScan, fieldBufferSize);
		if (numUpdatedPoints <= 0)
		{
			// < 0 will be an error code
			return numUpdatedPoints;
		}

	//	printf("# updated points: %d\n", numUpdatedPoints);
		d_compactUpdatedPoints = cl::Buffer(ctx->context, CL_MEM_READ_WRITE, numUpdatedPoints * sizeof(glm::ivec4));
		d_compactUpdatedMaterials = cl::Buffer(ctx->context, CL_MEM_READ_WRITE, numUpdatedPoints * sizeof(int));

		index = 0;
		cl::Kernel k_compact(meshGen->csgProgram.get(), "CompactPoints");
		CL_CALL(k_compact.setArg(index++, d_updatedIndices));
		CL_CALL(k_compact.setArg(index++, d_updatedPoints));
		CL_CALL(k_compact.setArg(index++, d_updatedMaterials));
		CL_CALL(k_compact.setArg(index++, d_updatedIndicesScan));
		CL_CALL(k_compact.setArg(index++, d_compactUpdatedPoints));
		CL_CALL(k_compact.setArg(index++, d_compactUpdatedMaterials));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_compact, cl::NullRange, fieldBufferSize, cl::NullRange));

		index = 0;
		cl::Kernel k_UpdateMaterials(meshGen->csgProgram.get(), "UpdateFieldMaterials");
		CL_CALL(k_UpdateMaterials.setArg(index++, d_compactUpdatedPoints));
		CL_CALL(k_UpdateMaterials.setArg(index++, d_compactUpdatedMaterials));
		CL_CALL(k_UpdateMaterials.setArg(index++, field.materials));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_UpdateMaterials, cl::NullRange, numUpdatedPoints, cl::NullRange));
	}

	unsigned int numCreatedEdges = 0;
	cl::Buffer d_createdEdges;

	unsigned int numInvalidatedEdges = 0;
	cl::Buffer d_invalidatedEdges;
	{
		rmt_ScopedCPUSample(Filter);

		const int numGeneratedEdges = numUpdatedPoints * 6;
	//	printf("# generated edges: %d\n", numGeneratedEdges);
		cl::Buffer d_generatedEdgeIndices(ctx->context, CL_MEM_READ_WRITE, numGeneratedEdges * sizeof(int));

		cl::Kernel k_findUpdatedEdges(meshGen->csgProgram.get(), "FindUpdatedEdges");
		CL_CALL(k_findUpdatedEdges.setArg(0, d_compactUpdatedPoints));
		CL_CALL(k_findUpdatedEdges.setArg(1, d_generatedEdgeIndices));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_findUpdatedEdges, cl::NullRange, numUpdatedPoints, cl::NullRange));

		// FindUpdatedEdges may generate an invalid edge when processing areas near the boundary,
		// in this cause a -1 will be written instead of the edge index, so we remove these invalid
		// indices via a scan and compact (not sure if the order of this and the subsequent 
		// RemoveDuplicates call matters performance wise?)
		cl::Buffer d_edgeIndicesValid(ctx->context, CL_MEM_READ_WRITE, numGeneratedEdges * sizeof(int));
		cl::Kernel k_removeInvalid(meshGen->csgProgram.get(), "RemoveInvalidIndices");
		CL_CALL(k_removeInvalid.setArg(0, d_generatedEdgeIndices));
		CL_CALL(k_removeInvalid.setArg(1, d_edgeIndicesValid));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_removeInvalid, cl::NullRange, numGeneratedEdges, cl::NullRange));

		cl::Buffer d_compactEdgeIndices;
		const int numCompactEdgeIndices = CompactIndexArray(ctx->queue, d_generatedEdgeIndices, 
			d_edgeIndicesValid, numGeneratedEdges, d_compactEdgeIndices);
		if (numCompactEdgeIndices < 0)
		{
			// i.e. its an error code
			return numCompactEdgeIndices;
		}

		d_invalidatedEdges = RemoveDuplicates(ctx->queue, d_compactEdgeIndices, numCompactEdgeIndices, &numInvalidatedEdges);
	//	printf("%d Generated %d unique\n", numGeneratedEdges, numInvalidatedEdges);

		cl::Buffer d_edgeValidity(ctx->context, CL_MEM_READ_WRITE, numInvalidatedEdges * sizeof(int));
		cl::Kernel k_FilterValid(meshGen->csgProgram.get(), "FilterValidEdges");
		CL_CALL(k_FilterValid.setArg(0, d_invalidatedEdges));
		CL_CALL(k_FilterValid.setArg(1, field.materials));
		CL_CALL(k_FilterValid.setArg(2, d_edgeValidity));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_FilterValid, cl::NullRange, numInvalidatedEdges, cl::NullRange));

		numCreatedEdges  = CompactIndexArray(ctx->queue, d_invalidatedEdges, 
			d_edgeValidity, numInvalidatedEdges, d_createdEdges);
	//	printf("%d created edges\n", numCreatedEdges);
	}

	if (numInvalidatedEdges > 0 && field.numEdges > 0)
	{
		rmt_ScopedCPUSample(Prune);

		cl::Buffer d_fieldEdgeValidity(ctx->context, CL_MEM_READ_WRITE, field.numEdges * sizeof(int));

		cl::Kernel k_PruneEdges(meshGen->csgProgram.get(), "PruneFieldEdges");
		CL_CALL(k_PruneEdges.setArg(0, field.edgeIndices));
		CL_CALL(k_PruneEdges.setArg(1, d_invalidatedEdges));
		CL_CALL(k_PruneEdges.setArg(2, numInvalidatedEdges));
		CL_CALL(k_PruneEdges.setArg(3, d_fieldEdgeValidity));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_PruneEdges, cl::NullRange, field.numEdges, cl::NullRange));

		cl::Buffer fieldEdgeScan(ctx->context, CL_MEM_READ_WRITE, field.numEdges * sizeof(int));
		const int numPrunedEdges = ExclusiveScan(ctx->queue, d_fieldEdgeValidity, fieldEdgeScan, field.numEdges);

		if (numPrunedEdges > 0)
		{
			cl::Buffer d_prunedEdgeIndices(ctx->context, CL_MEM_READ_WRITE, numPrunedEdges * sizeof(int));
			cl::Buffer d_prunedNormals(ctx->context, CL_MEM_READ_WRITE, numPrunedEdges * sizeof(glm::vec4));

			index = 0;
			cl::Kernel k_CompactEdges(meshGen->csgProgram.get(), "CompactFieldEdges");
			CL_CALL(k_CompactEdges.setArg(index++, d_fieldEdgeValidity));
			CL_CALL(k_CompactEdges.setArg(index++, fieldEdgeScan));
			CL_CALL(k_CompactEdges.setArg(index++, field.edgeIndices));
			CL_CALL(k_CompactEdges.setArg(index++, field.normals));
			CL_CALL(k_CompactEdges.setArg(index++, d_prunedEdgeIndices));
			CL_CALL(k_CompactEdges.setArg(index++, d_prunedNormals));
			CL_CALL(ctx->queue.enqueueNDRangeKernel(k_CompactEdges, cl::NullRange, field.numEdges, cl::NullRange));

			field.numEdges = numPrunedEdges;
			field.edgeIndices = d_prunedEdgeIndices;
			field.normals = d_prunedNormals;
		}
	}
	

	if (numCreatedEdges > 0)
	{
		rmt_ScopedCPUSample(Create);

		cl::Buffer d_createdNormals = cl::Buffer(ctx->context, CL_MEM_READ_WRITE, numCreatedEdges * sizeof(glm::vec4));

		index = 0;
		cl::Kernel k_FindEdgeInfo(meshGen->csgProgram.get(), "FindEdgeIntersectionInfo");
		CL_CALL(k_FindEdgeInfo.setArg(index++, fieldOffset));
		CL_CALL(k_FindEdgeInfo.setArg(index++, (u32)opInfo.size()));
		CL_CALL(k_FindEdgeInfo.setArg(index++, d_operations));
		CL_CALL(k_FindEdgeInfo.setArg(index++, sampleScale));
		CL_CALL(k_FindEdgeInfo.setArg(index++, d_createdEdges));
		CL_CALL(k_FindEdgeInfo.setArg(index++, d_createdNormals));
		CL_CALL(ctx->queue.enqueueNDRangeKernel(k_FindEdgeInfo, cl::NullRange, numCreatedEdges, cl::NullRange));

		if (field.numEdges > 0)
		{
			const unsigned int oldSize = field.numEdges;
			const unsigned int newSize = oldSize + numCreatedEdges;
			cl::Buffer combinedEdges(ctx->context, CL_MEM_READ_WRITE, newSize * sizeof(int));
			cl::Buffer combinedNormals(ctx->context, CL_MEM_READ_WRITE, newSize * sizeof(glm::vec4));

			CL_CALL(ctx->queue.enqueueCopyBuffer(field.edgeIndices, combinedEdges, 0, 0, oldSize * sizeof(int)));
			CL_CALL(ctx->queue.enqueueCopyBuffer(field.normals, combinedNormals, 0, 0, oldSize * sizeof(glm::vec4)));

			CL_CALL(ctx->queue.enqueueCopyBuffer(d_createdEdges, combinedEdges, 0, oldSize * sizeof(int), numCreatedEdges * sizeof(int)));
			CL_CALL(ctx->queue.enqueueCopyBuffer(d_createdNormals, combinedNormals, 0, oldSize * sizeof(glm::vec4), numCreatedEdges * sizeof(glm::vec4)));

			field.numEdges = newSize;
			field.edgeIndices = combinedEdges;
			field.normals = combinedNormals;
		}
		else
		{
			field.numEdges = numCreatedEdges;
			field.edgeIndices = d_createdEdges;
			field.normals = d_createdNormals;
		}
	}

	return CL_SUCCESS;
}

// ----------------------------------------------------------------------------

int Compute_ApplyCSGOperations(
	MeshGenerationContext* meshGen,
	const std::vector<CSGOperationInfo>& opInfo,
	const glm::ivec3& clipmapNodeMin,
	const int clipmapNodeSize)
{
//	printf("Apply: %d %d %d\n", clipmapNodeMin.x, clipmapNodeMin.y, clipmapNodeMin.z);
//	printf("  origin: %.1f %.1f %.1f\n", opInfo.origin.x, opInfo.origin.y, opInfo.origin.z);

	GPUDensityField field;
	CL_CALL(LoadDensityField(meshGen, clipmapNodeMin, clipmapNodeSize, &field));
	
	CL_CALL(ApplyCSGOperations(meshGen, opInfo, clipmapNodeMin, clipmapNodeSize, field));
	field.lastCSGOperation += opInfo.size();
	
	CL_CALL(StoreDensityField(meshGen, field));

	return CL_SUCCESS;
}