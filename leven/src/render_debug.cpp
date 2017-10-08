#include	"render_debug.h"

#include	"render_local.h"
#include	"render_program.h"
#include	"pool_allocator.h"
#include	"render_shapes.h"

#include	"sdl_wrapper.h"
#include	<glm/glm.hpp>
#include	<unordered_set>

using glm::vec4; 
using glm::vec3; 
using glm::mat4;

// ----------------------------------------------------------------------------

const int MAX_DEBUG_DRAW_CMD_BUFFERS = 1024;
IndexPoolAllocator g_debugDrawCmdAlloc;
DebugDrawBuffer* g_debugDrawCmdBuffers;
std::unordered_set<DebugDrawBuffer*> g_enabledDebugDrawBuffers;
GLSLProgram g_debugProgram;

void InitialiseDebugDraw()
{
	g_debugDrawCmdAlloc.initialise(MAX_DEBUG_DRAW_CMD_BUFFERS);
	g_debugDrawCmdBuffers = new DebugDrawBuffer[MAX_DEBUG_DRAW_CMD_BUFFERS];

	if (!g_debugProgram.initialise() ||
		!g_debugProgram.compileShader(GL_VERTEX_SHADER, "shaders/debug.vert") ||
		!g_debugProgram.compileShader(GL_FRAGMENT_SHADER, "shaders/debug.frag") ||
		!g_debugProgram.link())
	{
		// TODO proper error handling
		LVN_ALWAYS_ASSERT("Debug GLSL program build failed", false);
	}
}

// ----------------------------------------------------------------------------

void DestroyDebugDraw()
{
	g_debugDrawCmdAlloc.clear();
	delete[] g_debugDrawCmdBuffers;
	g_debugDrawCmdBuffers = nullptr;
}

// ----------------------------------------------------------------------------

int CreateDebugDrawBuffer(const int bufferIndex)
{
	LVN_ASSERT(bufferIndex >= 0 && bufferIndex < MAX_DEBUG_DRAW_CMD_BUFFERS);
	DebugDrawBuffer& buffer = g_debugDrawCmdBuffers[bufferIndex];

	glGenBuffers(1, &buffer.vertexBuffer);
	glGenBuffers(1, &buffer.colourBuffer);
	glGenBuffers(1, &buffer.indexBuffer);
	glGenVertexArrays(1, &buffer.vao);

	glBindVertexArray(buffer.vao);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffer.vertexBuffer);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffer.colourBuffer);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.indexBuffer);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int UpdateDebugDrawBuffer(const int bufferIndex, const RenderDebugCmdBuffer& cmdBuffer)
{
	LVN_ASSERT(bufferIndex >= 0 && bufferIndex < MAX_DEBUG_DRAW_CMD_BUFFERS);
	DebugDrawBuffer& buffer = g_debugDrawCmdBuffers[bufferIndex];

	int vertexBufferSize = 0;
	int indexBufferSize = 0;
	auto& cmds = cmdBuffer.commands();
	for (int i = cmds.size() - 1; i >= 0; i--)
	{
		int vertices = 0;
		int indices = 0;

		const auto& cmd = cmds[i];
		switch (cmd.shape)
		{
			case RenderShape_Cube:
				GetCubeDataSizes(&vertices, &indices);
				break;

			case RenderShape_Sphere:
				GetSphereDataSizes(&vertices, &indices);
				break;

			case RenderShape_Line:
				GetLineDataSizes(&vertices, &indices);
				break;
		}

		vertexBufferSize += vertices;
		indexBufferSize += indices;
	}

	vec4* vertexBuffer = new vec4[vertexBufferSize];
	vec4* colourBuffer = new vec4[vertexBufferSize];
	u16* indexBuffer = new u16[indexBufferSize];

	u32 numVertices = 0;
	u32 numIndices = 0;
	for (int i = cmds.size() - 1; i >= 0; i--)
	{
		auto& cmd = cmds[i];
		const vec4 colour = vec4(cmd.rgb, cmd.alpha);
		const u32 numVerticesBefore = numVertices;

		// TODO storing triangles and lines in the same buffer is broken unless we track it
		switch (cmd.shape)
		{
			case RenderShape_Cube:
			{
				const vec3 min(cmd.cube.min[0], cmd.cube.min[1], cmd.cube.min[2]);
				const vec3 max(cmd.cube.max[0], cmd.cube.max[1], cmd.cube.max[2]);
				GetCubeData(min, max, &vertexBuffer[numVertices], 
					&indexBuffer[numIndices], &numVertices, &numIndices);

				break;
			}

			case RenderShape_Sphere:
			{
				const vec3 origin(cmd.sphere.origin[0], cmd.sphere.origin[1], cmd.sphere.origin[2]);
				GetSphereData(origin, cmd.sphere.radius, 
					&vertexBuffer[numVertices], &indexBuffer[numIndices], 
					&numVertices, &numIndices);
				break;
			}

			case RenderShape_Line:
			{
				const vec3 start(cmd.line.start[0], cmd.line.start[1], cmd.line.start[2]);
				const vec3 end(cmd.line.end[0], cmd.line.end[1], cmd.line.end[2]);
				GetLineData(start, end, &vertexBuffer[numVertices], 
					&indexBuffer[numIndices], &numVertices, &numIndices);
				break;
			}
		}

		for (u32 i = numVerticesBefore; i < numVertices; i++)
		{
			colourBuffer[i] = colour; 
		}
	}

	LVN_ASSERT(numVertices == vertexBufferSize);
	LVN_ASSERT(numIndices == indexBufferSize);

	// TODO broken
	buffer.shape = cmds[0].shape;

	glBindBuffer(GL_ARRAY_BUFFER, buffer.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * buffer.numVertices, nullptr, GL_DYNAMIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * numVertices, &vertexBuffer[0], GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, buffer.colourBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * buffer.numVertices, nullptr, GL_DYNAMIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * numVertices, &colourBuffer[0], GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * buffer.numIndices, nullptr, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * numIndices, &indexBuffer[0], GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	buffer.numVertices = numVertices;
	buffer.numIndices = numIndices;

	delete[] vertexBuffer;
	delete[] colourBuffer;
	delete[] indexBuffer;;

	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int FreeDebugDrawBuffer(int bufferIndex)
{
	if (!bufferIndex || bufferIndex < 0 || bufferIndex >= MAX_DEBUG_DRAW_CMD_BUFFERS)
	{
		return LVN_ERR_INVALID_PARAM;
	}

	DebugDrawBuffer* buffer = &g_debugDrawCmdBuffers[bufferIndex];

	glDeleteBuffers(1, &buffer->vertexBuffer);
	glDeleteBuffers(1, &buffer->colourBuffer);
	glDeleteBuffers(1, &buffer->indexBuffer);
	glDeleteVertexArrays(1, &buffer->vao);

	buffer->vertexBuffer = 0;
	buffer->colourBuffer = 0;
	buffer->indexBuffer = 0;
	buffer->vao = 0;
	buffer->numIndices = 0;

	g_debugDrawCmdAlloc.free(&bufferIndex);

	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int Render_SetDebugDrawCmds(const int id, const RenderDebugCmdBuffer& cmds)
{
	if (id < 0 || id >= MAX_DEBUG_DRAW_CMD_BUFFERS || cmds.empty())
	{
		return LVN_ERR_INVALID_PARAM;
	}

	PushRenderCommand([=]()
	{
		return UpdateDebugDrawBuffer(id, cmds) == LVN_SUCCESS;
	});

	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int Render_AllocDebugDrawBuffer()
{
	const int allocIndex = g_debugDrawCmdAlloc.alloc();
	if (allocIndex != -1)
	{
		PushRenderCommand([=]()
		{
			return CreateDebugDrawBuffer(allocIndex) == LVN_SUCCESS;
		});

		DebugDrawBuffer* buffer = &g_debugDrawCmdBuffers[allocIndex];
		g_enabledDebugDrawBuffers.insert(&g_debugDrawCmdBuffers[allocIndex]);
	}

	return allocIndex;
}

// ----------------------------------------------------------------------------

int Render_FreeDebugDrawBuffer(int* id)
{
	if (!id || *id < 0 || *id >= MAX_DEBUG_DRAW_CMD_BUFFERS)
	{
		return LVN_ERR_INVALID_PARAM;
	}

	DebugDrawBuffer* buffer = &g_debugDrawCmdBuffers[*id];
	g_enabledDebugDrawBuffers.erase(buffer);
	PushRenderCommand([=]()
	{
		return FreeDebugDrawBuffer(*id) == LVN_SUCCESS;
	});

	*id = -1;
	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int Render_EnableDebugDrawBuffer(int id)
{
	if (id < 0 || id >= MAX_DEBUG_DRAW_CMD_BUFFERS)
	{
		return LVN_ERR_INVALID_PARAM;
	}

	g_enabledDebugDrawBuffers.insert(&g_debugDrawCmdBuffers[id]);
	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

int Render_DisableDebugDrawBuffer(int id)
{
	if (id < 0 || id >= MAX_DEBUG_DRAW_CMD_BUFFERS)
	{
		return LVN_ERR_INVALID_PARAM;
	}

	g_enabledDebugDrawBuffers.erase(&g_debugDrawCmdBuffers[id]);
	return LVN_SUCCESS;
}

// ----------------------------------------------------------------------------

std::vector<DebugDrawBuffer*> GetEnabledDebugDrawBuffers()
{
	std::vector<DebugDrawBuffer*> buffers(g_enabledDebugDrawBuffers.size());
	int i = 0;
	for (auto b: g_enabledDebugDrawBuffers)
	{
		buffers[i++] = b;
	}

	return buffers;
}

// ----------------------------------------------------------------------------

void DrawDebugBuffers(const mat4& projection, const mat4& worldView)
{
	GLSLProgramView view(&g_debugProgram);

	const mat4 model = mat4(1.f);
	g_debugProgram.setUniform("MVP", projection * (worldView * model));

	glPolygonMode(GL_FRONT, GL_FILL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const auto debugBuffers = GetEnabledDebugDrawBuffers();
	for (const auto& buffer: debugBuffers)
	{
		glBindVertexArray(buffer->vao);
		glDrawElements(buffer->shape != RenderShape_Line ? GL_TRIANGLES : GL_LINES, buffer->numIndices, GL_UNSIGNED_SHORT, (void*)0);
	}

	glBindVertexArray(0);
	glDisable(GL_BLEND);
}


// ----------------------------------------------------------------------------


