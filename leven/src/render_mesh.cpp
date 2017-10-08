#include	"render_mesh.h"

#include	"render_local.h"
#include	"pool_allocator.h"

#include	<mutex>
#include	<unordered_set>
#include	<Remotery.h>

const int MAX_RENDER_MESH = 8 * 1024;
IndexPoolAllocator g_renderMeshAlloc;
RenderMesh* g_renderMeshBuffer = nullptr;
std::unordered_set<RenderMesh*> g_activeMeshes;
std::mutex g_renderMeshMutex;

const int MAX_MESH_BUFFERS = 2 * 1024;
PoolAllocator<MeshBuffer> g_meshBufferAlloc;
std::mutex g_meshBufferMutex;

// ----------------------------------------------------------------------------

void InitialiseRenderMesh()
{
	g_renderMeshAlloc.initialise(MAX_RENDER_MESH);
	g_renderMeshBuffer = new RenderMesh[MAX_RENDER_MESH];
	g_activeMeshes.reserve(MAX_RENDER_MESH);

	g_meshBufferAlloc.initialise(MAX_MESH_BUFFERS);
}

// ----------------------------------------------------------------------------

void DestroyRenderMesh()
{
	g_meshBufferAlloc.clear();

	for (RenderMesh* mesh: g_activeMeshes)
	{
		mesh->destroy();
	}

	g_activeMeshes.clear();
	delete[] g_renderMeshBuffer;
	g_renderMeshBuffer = nullptr;
	g_renderMeshAlloc.clear();
}

// ----------------------------------------------------------------------------

MeshBuffer* Render_AllocMeshBuffer(const char* const tag)
{
	std::lock_guard<std::mutex> lock(g_meshBufferMutex);
	MeshBuffer* b = g_meshBufferAlloc.alloc();
	if (!b)
	{
		printf("Error! Could not alloc mesh buffer\n");
		return nullptr;
	}

	b->tag = tag;
	return b;
}

// ----------------------------------------------------------------------------

void Render_FreeMeshBuffer(MeshBuffer* buffer)
{
	std::lock_guard<std::mutex> lock(g_meshBufferMutex);
	g_meshBufferAlloc.free(buffer);
}

// ----------------------------------------------------------------------------

RenderMesh* AllocRenderMesh()
{
	RenderMesh* mesh = nullptr;

	std::lock_guard<std::mutex> lock(g_renderMeshMutex);
//	printf("Alloc mesh (%d alloc'd)\n", g_renderMeshAlloc.size()); 
	const int index = g_renderMeshAlloc.alloc();
	if (index != -1)
	{
		mesh = &g_renderMeshBuffer[index];

		LVN_ASSERT(g_activeMeshes.find(mesh) == end(g_activeMeshes));
		g_activeMeshes.insert(mesh);
	}

	return mesh;
}

// ----------------------------------------------------------------------------

static bool InitialiseMesh(
	RenderMesh* mesh,
	void (*initVertexArrayFn)(),
	const u32 vertexSize,
	MeshBuffer* buffer)
{
	rmt_ScopedCPUSample(InitMesh);

	// TODO setPosition broken since the vertices are specified with absolute/world coords!
	//	mesh->setPosition(position);
	mesh->uploadData(MeshBuffer::initialiseVertexArray, 
		sizeof(MeshVertex), buffer->numVertices, buffer->vertices, 
		buffer->numTriangles * 3, buffer->triangles);

	Render_FreeMeshBuffer(buffer);
	return true;			
}

// ----------------------------------------------------------------------------

RenderMesh* Render_AllocRenderMesh(const char* const tag, MeshBuffer* buffer, const glm::vec3& position)
{
	if (!buffer || buffer->numTriangles == 0)
	{
		return nullptr;
	}

	LVN_ASSERT(buffer->numTriangles > 0);
	LVN_ASSERT(buffer->numVertices > 0);

	RenderMesh* mesh = AllocRenderMesh();
	if (mesh)
	{
		PushRenderCommand(
			std::bind(InitialiseMesh, mesh, MeshBuffer::initialiseVertexArray, sizeof(MeshVertex), buffer));
	}
	else
	{
		Render_FreeMeshBuffer(buffer);
		printf("Error: unable to allocate render mesh [tag='%s']\n", tag);
	}

	return mesh;
}
// ----------------------------------------------------------------------------

RenderMesh* Render_AllocRenderMesh(const char* const tag, ActorMeshBuffer* buffer)
{
	if (!buffer || buffer->numIndices == 0)
	{
		return nullptr;
	}

	LVN_ASSERT(buffer->numIndices > 0);
	LVN_ASSERT(buffer->numVertices > 0);

	RenderMesh* mesh = AllocRenderMesh();
	if (mesh)
	{
		PushRenderCommand([=]()
		{
			mesh->uploadData(ActorMeshBuffer::initialiseVertexArray, 
				sizeof(ActorVertex), buffer->numVertices, buffer->vertices, 
				buffer->numIndices, buffer->indices);

			Render_ReleaseActorMeshBuffer(buffer);
			return true;			
		});
	}
	else
	{
		Render_ReleaseActorMeshBuffer(buffer);
		printf("Error: unable to allocate render mesh [tag='%s']\n", tag);
	}

	return mesh;
}

// ----------------------------------------------------------------------------

void Render_FreeRenderMesh(RenderMesh** mesh)
{
	const auto ptrOffset = *mesh - &g_renderMeshBuffer[0];
	LVN_ASSERT(ptrOffset >= 0 && ptrOffset < MAX_RENDER_MESH);

	{
		// TODO maybe a bit dodgy on x64?
		int index = ptrOffset & 0xffffffff;

		std::lock_guard<std::mutex> lock(g_renderMeshMutex);
		g_renderMeshAlloc.free(&index);
		g_activeMeshes.erase(*mesh);
	}

	*mesh = nullptr;
}

// ----------------------------------------------------------------------------

void MeshBuffer::initialiseVertexArray()
{
	rmt_ScopedCPUSample(InitVertexAttrib);

	// xyz
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 0);
	
	// normal
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)(sizeof(vec4) * 1));

	// colour
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)(sizeof(vec4) * 2));
}

// ----------------------------------------------------------------------------

void ActorMeshBuffer::initialiseVertexArray()
{
	rmt_ScopedCPUSample(InitVertexAttrib_Actor);

	// xyz
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(ActorVertex), 0);
}




