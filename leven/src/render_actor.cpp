#include	"render_actor.h"

#include	"render_local.h"
#include	"render_shapes.h"


ActorMeshBuffer* Render_CreateActorMesh(const RenderShape shape, const float size)
{
	ActorMeshBuffer* buffer = new ActorMeshBuffer;

	switch (shape)
	{
		case RenderShape_Cube:
			GetCubeDataSizes(&buffer->numVertices, &buffer->numIndices);
			break;

		case RenderShape_Sphere:
			GetSphereDataSizes(&buffer->numVertices, &buffer->numIndices);
			break;

		case RenderShape_Line:
		default:
			LVN_ASSERT(false);
			return nullptr;
	}

	vec4* vertexBuffer = new vec4[buffer->numVertices];
	u16* indexBuffer = new u16[buffer->numIndices];

	u32 dummy1 = 0, dummy2 = 0;
	switch (shape)
	{
		case RenderShape_Cube:
			GetCubeData(vec3(-size / 2.f), vec3(size / 2.f), 
				vertexBuffer, indexBuffer, &dummy1, &dummy2);
			break;

		case RenderShape_Sphere:
			GetSphereData(vec3(0.f), size / 2.f, 
				vertexBuffer, indexBuffer, &dummy1, &dummy2);
			break;

		case RenderShape_Line:
		default:
			LVN_ASSERT(false);
			return nullptr;
	}
	
	buffer->vertices = new ActorVertex[buffer->numVertices];
	buffer->indices = new int[buffer->numIndices];

	for (int i = 0; i < buffer->numVertices; i++)
	{
		buffer->vertices[i].pos = vertexBuffer[i];
	}

	for (int i = 0; i < buffer->numIndices; i++)
	{
		buffer->indices[i] = indexBuffer[i];
	}

	delete[] vertexBuffer;
	delete[] indexBuffer;

	return buffer;
}

void Render_ReleaseActorMeshBuffer(ActorMeshBuffer* buffer)
{
	if (!buffer)
	{
		return;
	}

	delete[] buffer->vertices;
	delete[] buffer->indices;
	delete buffer;
}

