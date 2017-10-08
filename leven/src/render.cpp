#include	"render.h"
#include	"render_program.h"
#include	"camera.h"
#include	"render_debug.h"
#include	"resource.h"
#include	"threadpool.h"
#include	"lrucache.h"
#include	"volume_materials.h"
#include	"pool_allocator.h"
#include	"render_local.h"

#include	<algorithm>
#include	<queue>
#include	<atomic>
#include	<mutex>
#include	<memory>
#include	<unordered_set>
#include	<Remotery.h>

#include	<glm/glm.hpp>
#include	<glm/ext.hpp>
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::mat4;

// ----------------------------------------------------------------------------

std::mutex g_cmdMutex;
std::queue<RenderCommand> g_renderCmds;

void PushRenderCommand(RenderCommand cmd)
{
	std::lock_guard<std::mutex> lock(g_cmdMutex);
	g_renderCmds.push(cmd);
}

// ----------------------------------------------------------------------------

RenderCommand PopRenderCommand()
{
	std::lock_guard<std::mutex> lock(g_cmdMutex);
	if (!g_renderCmds.empty())
	{
		RenderCommand cmd = g_renderCmds.front();
		g_renderCmds.pop();
		return cmd;
	}
	
	return nullptr;
}

// ----------------------------------------------------------------------------

void ResetRenderCommands()
{
	std::lock_guard<std::mutex> lock(g_cmdMutex);
	while (!g_renderCmds.empty())
	{
		g_renderCmds.pop();
	}
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

namespace {

RenderMesh* CreateSkyBoxMesh()
{
	const vec3 min(-100), max(100);
	// TODO dear god this is horrifie
    const float V[24*3] = 
	{
		// Front
		min.x, min.y, max.z,
		max.x, min.y, max.z,
		max.x, max.y, max.z,
		min.x, max.y, max.z,
		// Right
		max.x, min.y, max.z,
		max.x, min.y, min.z,
		max.x, max.y, min.z,
		max.x, max.y, max.z,
		// Back
		min.x, min.y, min.z,
		min.x, max.y, min.z,
		max.x, max.y, min.z,
		max.x, min.y, min.z,
		// Left
		min.x, min.y, max.z,
		min.x, max.y, max.z,
		min.x, max.y, min.z,
		min.x, min.y, min.z,
		// Bottom
		min.x, min.y, max.z,
		min.x, min.y, min.z,
		max.x, min.y, min.z,
		max.x, min.y, max.z,
		// Top
		min.x, max.y, max.z,
		max.x, max.y, max.z,
		max.x, max.y, min.z,
		min.x, max.y, min.z
    };

    const GLuint INDICES[] = 
	{
        0,1,2,0,2,3,
        4,5,6,4,6,7,
        8,9,10,8,10,11,
        12,13,14,12,14,15,
        16,17,18,16,18,19,
        20,21,22,20,22,23
    };

	const float NORMALS[24 * 3] =
	{
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
		0.f, 1.f, 0.f, 
	};

	MeshBuffer* buffer = Render_AllocMeshBuffer("skybox");
	buffer->numTriangles = 12;
	buffer->numVertices = 24;

	for (int i = 0; i < 24; i++)
	{
		buffer->vertices[i] = MeshVertex(
			vec4(V[i * 3 + 0], V[i * 3 + 1], V[i * 3 + 2], 0.f),
			vec4(0.f, 1.f, 0.f, 0.f),
			vec4(0.1f, 0.1f, 0.7f, 0.f));
	}

	for (int i = 0; i < 12; i++)
	{
		buffer->triangles[i] = MeshTriangle(
			INDICES[i * 3 + 0],
			INDICES[i * 3 + 2],
			INDICES[i * 3 + 1]);
	}

	RenderMesh* mesh = new RenderMesh;
	mesh->uploadData(MeshBuffer::initialiseVertexArray, 
		sizeof(MeshVertex), buffer->numVertices, buffer->vertices, 
		buffer->numTriangles * 3, buffer->triangles);

	Render_FreeMeshBuffer(buffer);

	return mesh;
}

// ----------------------------------------------------------------------------

class Renderer
{
public:
	
	Renderer()
		: screenWidth_(0)
		, screenHeight_(0)
		, wireframe_(false)
		, materialLookupTexture_(0)
		, materialLookupBuffer_(0)
		, useShadowView_(false)
		, enableDrawUI_(false)
		, debugVAO_(0)
		, debugVBuffer_(0)
		, debugIBuffer_(0)
		, debugNumIndices_(0)
		, skyboxMesh_(nullptr)
		, quadVAO_(0)
		, deferredFBO_(0)
		, enableLight_(true)
		, showNormals_(false)
		, worldMin_(0)
		, worldMax_(256)
		, shadowMapSize_(2048)
		, shadowFBO_(0)
		, shadowMap_(0)
		, uiVAO_(0)
		, uiNumIndices_(0)
	{
	}

	// ----------------------------------------------------------------------------

	void initialiseShadowMapMatrices()
	{
		const vec3 centre = Camera_GetPosition();
		const float distanceFromCentre = 1.f;

		shadowView_ = glm::lookAt(vec3(centre) - (shadowDir_ * distanceFromCentre), vec3(centre), vec3(0,1,0));

		vec4 worldBoundingPoints[8] =
		{
			vec4(worldMin_.x, worldMin_.y, worldMin_.z, 1),
			vec4(worldMax_.x, worldMin_.y, worldMin_.z, 1),
			vec4(worldMax_.x, worldMin_.y, worldMax_.z, 1),
			vec4(worldMin_.x, worldMax_.y, worldMin_.z, 1),
			vec4(worldMax_.x, worldMax_.y, worldMin_.z, 1),
			vec4(worldMax_.x, worldMax_.y, worldMax_.z, 1),
			vec4(worldMin_.x, worldMax_.y, worldMax_.z, 1)
		};

		for (int i = 0; i < 8; i++)
		{
			worldBoundingPoints[i] = shadowView_ * worldBoundingPoints[i];
		}

		vec4 min(FLT_MAX), max(-1.f * FLT_MAX);
		for (int i = 0; i < 8; i++)
		{
			const vec4 p = vec4(worldBoundingPoints[i]);
			min.x = glm::min(min.x, p.x);
			max.x = glm::max(max.x, p.x);

			min.y = glm::min(min.y, p.y);
			max.y = glm::max(max.y, p.y);

			min.z = glm::min(min.z, p.z);
			max.z = glm::max(max.z, p.z);
		}

		shadowProj_ = glm::ortho<float>(min.x, max.x, min.y, max.y, -max.z, -min.z);
	}

	// ----------------------------------------------------------------------------
 
	bool initialise(
		const bool useShadows, 
		const int shadowMapSize,
		const ViewParams& viewParams,
		MaterialSet& materials)
	{
		shadowMapSize_ = shadowMapSize;

		if (!pass1Program_.initialise() ||
		    !actorProgram_.initialise())
		{
			return false;
		}

		pass1Program_.prependLine("#define VOXEL_DRAW 0");
		pass1Program_.prependLine("#define ACTOR_DRAW 1");
		pass1Program_.prependLine("#define DRAW_MODE VOXEL_DRAW");

		actorProgram_.prependLine("#define VOXEL_DRAW 0");
		actorProgram_.prependLine("#define ACTOR_DRAW 1");
		actorProgram_.prependLine("#define DRAW_MODE ACTOR_DRAW");

		char str[1024];
		sprintf(str, "#define FRUSTUM_NEAR (%f)", viewParams.nearDistance);
		pass1Program_.prependLine(str);
		actorProgram_.prependLine(str);

		sprintf(str, "#define FRUSTUM_FAR (%f)", viewParams.farDistance);
		pass1Program_.prependLine(str);
		actorProgram_.prependLine(str);

		shadowDir_ = glm::normalize(vec3(-0.7f, -0.7f, -0.3f));

		const mat4 viewportMatrix = mat4(
			vec4(screenWidth_ / 2.f, 0.f, 0.f, 0.f),
			vec4(0.f, screenHeight_ / 2.f, 0.f, 0.f),
			vec4(0.f, 0.f, 1.f, 0.f),
			vec4(screenWidth_ / 2.f, screenHeight_ / 2.f, 0.f, 1.f));

		useShadows_ = useShadows;
		if (useShadows_)
		{
			actorProgram_.prependLine("#define USE_SHADOWS");
			initialiseShadowMapMatrices();
		}

		if (!pass1Program_.compileShader(GL_VERTEX_SHADER, "shaders/pass1.vert") ||
			!pass1Program_.compileShader(GL_GEOMETRY_SHADER, "shaders/wireframe.geo") ||
			!pass1Program_.compileShader(GL_FRAGMENT_SHADER, "shaders/pass1.frag") ||
			!pass1Program_.link())
		{
			return false;
		}
		else
		{
			GLSLProgramView view(&pass1Program_);

			projection_ = glm::perspective(viewParams.fov, viewParams.aspectRatio, 
				viewParams.nearDistance, viewParams.farDistance);
			pass1Program_.setUniform("projectionMatrix", projection_);

			const glm::vec3 lightPos(0, 256, 0);
			pass1Program_.setUniform("lightPos", glm::normalize(lightPos));
			pass1Program_.setUniform("u_viewportMatrix", viewportMatrix);
		}

		if (!actorProgram_.compileShader(GL_VERTEX_SHADER, "shaders/pass1.vert") ||
			!actorProgram_.compileShader(GL_GEOMETRY_SHADER, "shaders/wireframe.geo") ||
			!actorProgram_.compileShader(GL_FRAGMENT_SHADER, "shaders/pass1.frag") ||
			!actorProgram_.link())
		{
			return false;
		}
		else
		{
			GLSLProgramView view(&actorProgram_);

			projection_ = glm::perspective(viewParams.fov, viewParams.aspectRatio, 
				viewParams.nearDistance, viewParams.farDistance);
			actorProgram_.setUniform("projectionMatrix", projection_);

			const glm::vec3 lightPos(0, 256, 0);
			actorProgram_.setUniform("lightPos", glm::normalize(lightPos));
			actorProgram_.setUniform("u_viewportMatrix", viewportMatrix);
		}

		if (!pass2Program_.initialise())
		{
			return false;
		}

		if (!pass2Program_.compileShader(GL_VERTEX_SHADER, "shaders/pass2.vert") ||
			!pass2Program_.compileShader(GL_FRAGMENT_SHADER, "shaders/pass2.frag") ||
			!pass2Program_.link())
		{
			return false;
		}
		else
		{
			GLSLProgramView view(&pass2Program_);

			pass2Program_.setUniformFloat("enableLight", enableLight_ ? 1.f : 0.f);
			pass2Program_.setUniformInt("showNormals", showNormals_ ? 1 : 0);

			pass2Program_.setUniform("lightDir", glm::normalize(shadowDir_));

			pass2Program_.setUniformFloat("screenWidth", static_cast<float>(screenWidth_));
			pass2Program_.setUniformFloat("screenHeight", static_cast<float>(screenHeight_));
		}

		if (!shadowProgram_.initialise() ||
			!shadowProgram_.compileShader(GL_VERTEX_SHADER, "shaders/shadowmap.vert") ||
			!shadowProgram_.compileShader(GL_FRAGMENT_SHADER, "shaders/shadowmap.frag") ||
			!shadowProgram_.link())
		{
			return false;
		}

		if (!uiProgram_.initialise() ||
			!uiProgram_.compileShader(GL_VERTEX_SHADER, "shaders/ui.vert") ||
			!uiProgram_.compileShader(GL_FRAGMENT_SHADER, "shaders/ui.frag") ||
			!uiProgram_.link())
		{
			return false;
		}

		if (!skyboxProgram_.initialise() ||
			!skyboxProgram_.compileShader(GL_VERTEX_SHADER, "shaders/skybox.vert") ||
			!skyboxProgram_.compileShader(GL_FRAGMENT_SHADER, "shaders/skybox.frag") ||
			!skyboxProgram_.link())
		{
			return false;
		}

		uiProjection_ = glm::ortho<float>(0, screenWidth_, screenHeight_, 0, 0.f, 1.f);
		uiView_ = mat4(1);


		// Array for quad
		GLfloat verts[] = {
			-1.0f, -1.0f, 0.0f, 
			1.0f, -1.0f, 0.0f, 
			1.0f, 1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f, 
			1.0f, 1.0f, 0.0f, 
			-1.0f, 1.0f, 0.0f
		};
		GLfloat tc[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f
		};


		GLuint quadBuffers[2];
		glGenBuffers(2, &quadBuffers[0]);

		glBindBuffer(GL_ARRAY_BUFFER, quadBuffers[0]);
		glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(float), verts, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, quadBuffers[1]);
		glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), tc, GL_STATIC_DRAW);

		glGenVertexArrays(1, &quadVAO_);
		glBindVertexArray(quadVAO_);


		glBindBuffer(GL_ARRAY_BUFFER, quadBuffers[0]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);
		
		glBindBuffer(GL_ARRAY_BUFFER, quadBuffers[1]);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(4);

		glBindVertexArray(0);


		setupFBOs();

		if (useShadows_)
		{
			setupShadowMapFBO();
		}

		textureArrayID_ = materials.bakeTextureArray();
		const std::vector<float> materialTextures = materials.exportMaterialTextures();
		LVN_ASSERT(materialTextures.size() % 3 == 0);

		glGenBuffers(1, &materialLookupBuffer_);
		glBindBuffer(GL_TEXTURE_BUFFER, materialLookupBuffer_);
		glBufferData(GL_TEXTURE_BUFFER, sizeof(float) * materialTextures.size(), &materialTextures[0], GL_STATIC_DRAW);

		glGenTextures(1, &materialLookupTexture_);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);

		glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

		const GLint crosshairTexture = Resource_LoadTexture("assets/crosshair.png");
		UIElementPtr crosshair(new UIElement(vec2(screenWidth_/2, screenHeight_/2), vec2(16,16), crosshairTexture));
		uiElements_.push_back(std::move(crosshair));

		// TODO material preview is broken due to the texture array
		/*
		const float previewSize = 128.f;
		materialPreview_ = std::make_shared<UIElement>(
			vec2(previewSize / 2.f, screenHeight_ - (previewSize / 2.f)), vec2(previewSize), 2);
		uiElements_.push_back(materialPreview_);
		*/

		skyboxMesh_ = CreateSkyBoxMesh();

		createUIGeometry();

		return true;
	}

	// ----------------------------------------------------------------------------

	void createGBufferTexture(GLenum textureUnit, GLenum format, GLuint& id)
	{
		glActiveTexture(textureUnit);
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glTexStorage2D(GL_TEXTURE_2D, 1, format, screenWidth_, screenHeight_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// ----------------------------------------------------------------------------

	void setupFBOs()
	{
		glGenFramebuffers(1, &deferredFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, deferredFBO_);

		glGenTextures(1, &depthBuffer_);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthBuffer_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth_, screenHeight_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBuffer_, 0);

		createGBufferTexture(GL_TEXTURE1, GL_RGBA32F, normalTex_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, normalTex_, 0);

		createGBufferTexture(GL_TEXTURE2, GL_RGB32F, colourTex_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, colourTex_, 0);

		glActiveTexture(GL_TEXTURE3);
		glGenTextures(1, &linearDepthTexture_);
		glBindTexture(GL_TEXTURE_2D, linearDepthTexture_);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, screenWidth_, screenHeight_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, linearDepthTexture_, 0);

		GLenum drawBuffers[] = { GL_NONE, GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers(4, drawBuffers);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("Frame buffer incomplete\n");
			exit(EXIT_FAILURE);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void setupShadowMapFBO()
	{
		// Shadow mapping FBO
		glGenTextures(1, &shadowMap_);
		glBindTexture(GL_TEXTURE_2D, shadowMap_);
	//	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, shadowMapSize_, shadowMapSize_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexStorage2D(GL_TEXTURE_2D, 11, GL_DEPTH_COMPONENT32F, shadowMapSize_, shadowMapSize_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	//	GLfloat border[4] = { 1.f, 0.f, 0.f, 0.f };
	//	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

		glGenFramebuffers(1, &shadowFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMap_, 0);

		GLuint shadowDrawBuffers[] = { GL_NONE };
		glDrawBuffers(1, shadowDrawBuffers);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void setViewport(int w, int h)
	{
		screenWidth_ = w;
		screenHeight_ = h;
		glViewport(0, 0, w, h);
	}

	// ----------------------------------------------------------------------------

	void sortMeshes(std::vector<RenderMesh*>& visibleMeshes)
	{
		const vec3 pos = Camera_GetPosition();
		std::sort(begin(visibleMeshes), end(visibleMeshes), 
			[&](const RenderMesh* lhs, const RenderMesh* rhs)
			{
				const float d1 = glm::length2(lhs->getPosition() - pos);	
				const float d2 = glm::length2(rhs->getPosition() - pos);	
				return d1 < d2;
			}
		);
	}

	// ----------------------------------------------------------------------------

	void drawPass2(const vec3& cameraPosition)
	{
		GLSLProgramView programView(&pass2Program_);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, normalTex_);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, colourTex_);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, linearDepthTexture_);

		pass2Program_.setUniform("worldToCameraMatrix", mat4(1.f));
		pass2Program_.setUniform("modelToWorldMatrix", mat4(1.f));
		pass2Program_.setUniform("projectionMatrix", projection_);
		pass2Program_.setUniformInt("u_useTextures", wireframe_ ? 0 : 1);
		pass2Program_.setUniformInt("u_showWireframe", wireframe_ ? 1 : 0);
		pass2Program_.setUniform("cameraPosition", cameraPosition);

		glBindVertexArray(quadVAO_);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// ----------------------------------------------------------------------------

	void drawShadowFrame(
		const std::vector<RenderMesh*>& visibleMeshes,
		const std::vector<RenderMesh*>& visibleActorMeshes)
	{
		GLSLProgramView programView(&shadowProgram_);

		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
		glViewport(0, 0, shadowMapSize_, shadowMapSize_);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1e-3f, 1e-3f);
		glClear(GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		shadowProgram_.setUniform("modelToWorldMatrix", mat4(1.f));
		shadowProgram_.setUniform("projectionMatrix", shadowProj_);
		shadowProgram_.setUniform("worldToCameraMatrix", shadowView_);

		drawMeshes(shadowProgram_, visibleMeshes);
		drawMeshes(shadowProgram_, visibleActorMeshes);

		glDisable(GL_POLYGON_OFFSET_FILL);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// ----------------------------------------------------------------------------

	void drawMeshes(GLSLProgram& program, const std::vector<RenderMesh*>& meshes)
	{
		for (RenderMesh* mesh: meshes)
		{
			mesh->draw(program);
		}
	}

	// ----------------------------------------------------------------------------

	void drawGeometryFrame(
		const vec3& cameraPos, 
		const std::vector<RenderMesh*>& visibleMeshes,
		const std::vector<RenderMesh*>& visibleActorMeshes)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, deferredFBO_);

		glViewport(0, 0, screenWidth_, screenHeight_);
		glLineWidth(.5f);
		glClearColor(0.3f, 0.f, 0.6f, 0.f); 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.f, 1.f, 0.f, 0.f); 

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		glEnable(GL_DEPTH_TEST);

		drawSkyBox(cameraPos);

		{
			GLSLProgramView programView(&pass1Program_);

			if (!useShadowView_)
			{
				pass1Program_.setUniform("worldToCameraMatrix", worldView_);
				pass1Program_.setUniform("projectionMatrix", projection_);
			}
			else
			{
				pass1Program_.setUniform("worldToCameraMatrix", shadowView_);
				pass1Program_.setUniform("projectionMatrix", shadowProj_);
			}

			const mat4 shadowBias = mat4(
				vec4(0.5f, 0.0f, 0.0f, 0.0f),
				vec4(0.0f, 0.5f, 0.0f, 0.0f),
				vec4(0.0f, 0.0f, 0.5f, 0.0f),
				vec4(0.5f, 0.5f, 0.5f, 1.0f));

			if (useShadows_)
			{
				pass1Program_.setUniform("shadowMVP", shadowBias * (shadowProj_ * shadowView_));
			}

			pass1Program_.setUniform("u_cameraPosition", cameraPos);

			glm::mat4 modelMatrix(1.f);

			glm::mat4 modelViewMatrix = worldView_ * modelMatrix;
			glm::mat3 normalMatrix = glm::mat3(1.f);

			const bool adjustNormalsForView = false;
			if (adjustNormalsForView)
			{
				normalMatrix = glm::mat3(glm::vec3(worldView_[0]),
					glm::vec3(worldView_[1]), glm::vec3(worldView_[2]));
			}

			if (useShadows_)
			{
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, shadowMap_);
			}
			
			pass1Program_.setUniform("normalMatrix", normalMatrix);
			pass1Program_.setUniformInt("u_useTextures", wireframe_ ? 0 : 1);
			pass1Program_.setUniformInt("u_showWireframe", wireframe_ ? 1 : 0);
			pass1Program_.setUniformFloat("textureScale", 1 / 32.f);
			pass1Program_.setUniform("modelToWorldMatrix", modelMatrix);
			
			glEnable(GL_TEXTURE_1D);
			glEnable(GL_TEXTURE_2D);
			glEnable(GL_TEXTURE_2D_ARRAY);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_BUFFER, materialLookupTexture_);
			glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, materialLookupBuffer_);

			glPolygonMode(GL_FRONT, GL_FILL);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayID_);

			drawMeshes(pass1Program_, visibleMeshes);

			glDisable(GL_TEXTURE_2D_ARRAY);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_1D);
		}

		{
			GLSLProgramView programView(&actorProgram_);

			if (!useShadowView_)
			{
				actorProgram_.setUniform("worldToCameraMatrix", worldView_);
				actorProgram_.setUniform("projectionMatrix", projection_);
			}
			else
			{
				actorProgram_.setUniform("worldToCameraMatrix", shadowView_);
				actorProgram_.setUniform("projectionMatrix", shadowProj_);
			}

			const mat4 shadowBias = mat4(
				vec4(0.5f, 0.0f, 0.0f, 0.0f),
				vec4(0.0f, 0.5f, 0.0f, 0.0f),
				vec4(0.0f, 0.0f, 0.5f, 0.0f),
				vec4(0.5f, 0.5f, 0.5f, 1.0f));

			if (useShadows_)
			{
				actorProgram_.setUniform("shadowMVP", shadowBias * (shadowProj_ * shadowView_));
			}

			glm::mat4 modelMatrix(1.f);

			glm::mat4 modelViewMatrix = worldView_ * modelMatrix;
			glm::mat3 normalMatrix = glm::mat3(1.f);

			const bool adjustNormalsForView = false;
			if (adjustNormalsForView)
			{
				normalMatrix = glm::mat3(glm::vec3(worldView_[0]),
					glm::vec3(worldView_[1]), glm::vec3(worldView_[2]));
			}

			if (useShadows_)
			{
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, shadowMap_);
			}
			
			actorProgram_.setUniform("normalMatrix", normalMatrix);
			actorProgram_.setUniform("modelToWorldMatrix", modelMatrix);
			
			glPolygonMode(GL_FRONT, GL_FILL);

			drawMeshes(actorProgram_, visibleActorMeshes);
		}

	}

	// ----------------------------------------------------------------------------

	void debugToggleLight()
	{
		enableLight_ = !enableLight_;

		GLSLProgramView view(&pass2Program_);
		pass2Program_.setUniformFloat("enableLight", enableLight_ ? 1.f : 0.f);
	}

	// ----------------------------------------------------------------------------

	void debugRandomLightDir()
	{
		shadowDir_ = glm::normalizedRand3(-1.f, 1.f);
		shadowDir_.y = shadowDir_.y > 0.f ? -shadowDir_.y : shadowDir_.y;

		GLSLProgramView view(&pass2Program_);
		pass2Program_.setUniform("lightDir", shadowDir_);
	}

	// ----------------------------------------------------------------------------

	void debugShowNormals()
	{
		showNormals_ = !showNormals_;

		{
			GLSLProgramView view(&pass1Program_);
			pass1Program_.setUniformInt("showNormals", showNormals_ ? 1 : 0);
		}

		{
			GLSLProgramView view(&actorProgram_);
			actorProgram_.setUniformInt("showNormals", showNormals_ ? 1 : 0);
		}

		{
			GLSLProgramView view(&pass2Program_);
			pass2Program_.setUniformInt("showNormals", showNormals_ ? 1 : 0);
		}
	}

	// ----------------------------------------------------------------------------

	void addDebugCube(const glm::vec3& min, const glm::vec3& max)
	{
		if (debugShapes_.size() < 8192)
		{
			debugShapes_.push_back(
				std::make_pair<const glm::vec3&, const glm::vec3&>(min, max)
			);
		}
	}

	// ----------------------------------------------------------------------------

	void createUIGeometry()
	{
		const int MAX_UI_ELEMENTS = 10;
		static vec3	vertexBuffer[6 * MAX_UI_ELEMENTS];
		static vec2 uvBuffer[6 * MAX_UI_ELEMENTS];

		int numVertices = 0, numIndices = 0;

		// Array for quad
		const vec3 verts[] = {
			vec3(-1.0f, -1.0f, 0.0f), 
			vec3(1.0f, -1.0f, 0.0f), 
			vec3(1.0f, 1.0f, 0.0f),
			vec3(-1.0f, -1.0f, 0.0f), 
			vec3(1.0f, 1.0f, 0.0f), 
			vec3(-1.0f, 1.0f, 0.0f),
		};

		const vec2 uv[] = {
			vec2(0.0f, 0.0f),
			vec2(1.0f, 0.0f),
			vec2(1.0f, 1.0f),
			vec2(0.0f, 0.0f),
			vec2(1.0f, 1.0f),
			vec2(0.0f, 1.0f),
		};

		std::for_each(begin(uiElements_), end(uiElements_), 
			[&](const UIElementPtr& e)
			{
				const float halfx = e->dim_.x / 2.f;	
				const float halfy = e->dim_.y / 2.f;	

				for (int i = 0; i < 6; i++)
				{
					vertexBuffer[numVertices] = vec3(e->pos_, 0.f) + (verts[i] * vec3(halfx, halfy, 0));
					uvBuffer[numVertices] = uv[i];

					numVertices++;
				}
			}
		);

		glGenBuffers(2, &uiBuffers_[0]);

		glBindBuffer(GL_ARRAY_BUFFER, uiBuffers_[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * numVertices, &vertexBuffer[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, uiBuffers_[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * numVertices, &uvBuffer[0], GL_STATIC_DRAW);

		uiNumIndices_ = numVertices;

		glGenVertexArrays(1, &uiVAO_);
		glBindVertexArray(uiVAO_);

		glBindBuffer(GL_ARRAY_BUFFER, uiBuffers_[0]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, uiBuffers_[1]);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	// ----------------------------------------------------------------------------

	void drawSkyBox(const vec3& cameraPos)
	{
		GLSLProgramView view(&skyboxProgram_);

		glPolygonMode(GL_FRONT, GL_FILL);

		const mat4 skyboxModelMatrix = glm::translate(cameraPos);
		const mat4 skyboxMVP = projection_ * (worldView_ * skyboxModelMatrix);
		skyboxProgram_.setUniform("MVP", skyboxMVP);

		glDepthMask(GL_FALSE);
		skyboxMesh_->draw(skyboxProgram_);
		glDepthMask(GL_TRUE);
	}

	// ----------------------------------------------------------------------------

	void drawUI()
	{
		GLSLProgramView view(&uiProgram_);
		
		uiProgram_.setUniform("MVP", uiProjection_ * uiView_);

		glPolygonMode(GL_FRONT, GL_FILL);
		glEnable(GL_TEXTURE_2D);

		glDisable(GL_CULL_FACE);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		int counter = 0;
		std::for_each(begin(uiElements_), end(uiElements_), [&](const UIElementPtr& element)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, element->textureID_);

			glBindVertexArray(uiVAO_);	
			glDrawArrays(GL_TRIANGLES, counter * 6, counter * 6 + 6);

			counter++;
		});

		glBindVertexArray(0);

		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

	// ----------------------------------------------------------------------------

	void drawFrame(
		const std::vector<RenderMesh*>& visibleMeshes,
		const std::vector<RenderMesh*>& visibleActorMeshes)
	{
		const glm::vec3& cameraPos = Camera_GetPosition();
		const glm::vec3& cameraForward = Camera_GetForward();
		worldView_ = glm::lookAt(cameraPos + cameraForward, cameraPos, glm::vec3(0.f, 1.f, 0.f));

		if (useShadows_)
		{
			initialiseShadowMapMatrices();
			drawShadowFrame(visibleMeshes, visibleActorMeshes);
		}

		drawGeometryFrame(cameraPos, visibleMeshes, visibleActorMeshes);
		drawPass2(cameraPos);

		DrawDebugBuffers(projection_, worldView_);

		if (enableDrawUI_)
		{
			drawUI();
		}

	}

	// ----------------------------------------------------------------------------


	GLSLProgram		pass1Program_;
	GLSLProgram     actorProgram_;
	GLSLProgram		pass2Program_;
	glm::mat4		projection_, worldView_;
	int				screenWidth_, screenHeight_;

	GLuint			materialLookupTexture_;
	GLuint			materialLookupBuffer_;

	std::vector<std::pair<glm::vec3, glm::vec3>> debugShapes_;
	GLuint			debugVAO_, debugVBuffer_, debugIBuffer_;
	unsigned int	debugNumIndices_;

	GLSLProgram		skyboxProgram_;
	RenderMesh*		skyboxMesh_;

	// view options
	bool			wireframe_;
	bool			useShadowView_;
	bool			enableDrawUI_;

	GLuint			quadVAO_;

	// Deferred render buffers
	GLuint			deferredFBO_;
	GLuint			depthBuffer_, normalTex_, colourTex_;
	GLuint			linearDepthTexture_;

	bool			enableLight_;
	bool			showNormals_;

	// Shadow mapping
	GLSLProgram		shadowProgram_;
	bool			useShadows_;
	GLuint			shadowMapSize_;
	ivec3			worldMin_, worldMax_;

	GLuint			shadowFBO_;
	GLuint			shadowMap_;
	vec3			shadowDir_;

	mat4			shadowProj_, shadowView_;

	// UI pass
	GLSLProgram		uiProgram_;
	mat4			uiProjection_, uiView_;
	GLuint			uiVAO_;
	GLuint			uiBuffers_[3];
	int				uiNumIndices_;


	class UIElement
	{
	public:

		UIElement(const vec2& p, const vec2& d, GLint textureID)
			: pos_(p)
			, dim_(d)
			, textureID_(textureID)
		{
		}

		vec2		pos_;
		vec2		dim_;
		GLint		textureID_;
	};

	typedef std::shared_ptr<UIElement> UIElementPtr;
	std::vector<UIElementPtr>	uiElements_;
	UIElementPtr				materialPreview_;

	// Materials
	GLuint						textureArrayID_;
};

Renderer renderer;



}	// namespace

// ----------------------------------------------------------------------------

bool Render_Initialise(
	const int width, 
	const int height,
	const bool useShadows, 
	const int shadowMapSize,
	const ViewParams& viewParams,
	MaterialSet& materials)
{
	renderer.setViewport(width, height);

	InitialiseRenderMesh();
	InitialiseDebugDraw();

	return renderer.initialise(useShadows, shadowMapSize, viewParams, materials);
}

// ----------------------------------------------------------------------------

std::atomic<int> g_renderFrameCount = 0;

bool Render_DispatchCommands()
{
	rmt_ScopedCPUSample(Render_DispatchCmds);

	int count = 0;

	g_renderFrameCount++;

	while (RenderCommand cmd = PopRenderCommand())
	{
		cmd();
	}

	return true;
}

// ----------------------------------------------------------------------------

void Render_Shutdown()
{
	DestroyRenderMesh();
	DestroyDebugDraw();
}

// ----------------------------------------------------------------------------

std::mutex g_frameMeshMutex;
std::vector<RenderMesh*> g_frameMeshList;
std::vector<RenderMesh*> g_frameActorMeshList;
int g_frameNumTriangles = 0;

void Render_FrameBegin()
{
	std::lock_guard<std::mutex> lock(g_frameMeshMutex);
	g_frameMeshList.clear();
	g_frameActorMeshList.clear();
	g_frameNumTriangles = 0;
}

// ----------------------------------------------------------------------------

void Render_FrameAddMesh(RenderMesh* mesh)
{
	std::lock_guard<std::mutex> lock(g_frameMeshMutex);
	g_frameMeshList.push_back(mesh);
	g_frameNumTriangles += mesh->numTriangles();
}

// ----------------------------------------------------------------------------

void Render_FrameAddActorMesh(RenderMesh* mesh)
{
	std::lock_guard<std::mutex> lock(g_frameMeshMutex);
	g_frameActorMeshList.push_back(mesh);
	g_frameNumTriangles += mesh->numTriangles();
}

// ----------------------------------------------------------------------------

void Render_DrawFrame()
{
	rmt_ScopedCPUSample(Render_DrawFrame);
	renderer.drawFrame(g_frameMeshList, g_frameActorMeshList);
}

// ----------------------------------------------------------------------------

void Render_FrameEnd(int* numTriangles)
{
	*numTriangles = g_frameNumTriangles;
}

// ----------------------------------------------------------------------------

void Render_UnprojectPoint(const ivec2& point, vec3& worldspaceNear, vec3& worldspaceFar)
{
	const vec3 near(point.x, point.y, 0.f);
	const vec3 far(point.x, point.y, 1.f);
	const vec4 viewport(0.f, 0.f, renderer.screenWidth_, renderer.screenHeight_);

	worldspaceNear = glm::unProject(near, renderer.worldView_, renderer.projection_, viewport);
	worldspaceFar = glm::unProject(far, renderer.worldView_, renderer.projection_, viewport);
}

// ----------------------------------------------------------------------------

const glm::mat4& Render_GetProjectionMatrix()
{
	return renderer.projection_;
}

// ----------------------------------------------------------------------------

const glm::mat4& Render_GetWorldViewMatrix()
{
	return renderer.worldView_;
}

int Render_GetScreenWidth()
{
	return renderer.screenWidth_;
}

int Render_GetScreenHeight()
{
	return renderer.screenHeight_;
}

void Render_ToggleWireframe()
{
	renderer.wireframe_ = !renderer.wireframe_;
}

void Render_ToggleShadowView()
{
	renderer.useShadowView_ = !renderer.useShadowView_;
}

void Render_SetDrawUI(const bool enable)
{
	renderer.enableDrawUI_ = enable;
}

void Render_ToggleLighting()
{
	renderer.debugToggleLight();
}

void Render_ToggleNormals()
{
	renderer.debugShowNormals();
}

// ----------------------------------------------------------------------------

void Render_RandomLightDir()
{
	renderer.debugRandomLightDir();
}

// ----------------------------------------------------------------------------

void Render_Reset()
{
	ResetRenderCommands();

	DestroyRenderMesh();
	InitialiseRenderMesh();
}

// ----------------------------------------------------------------------------

void Render_UpdatePreviewMaterial(const int materialID)
{
	if (renderer.materialPreview_)
	{
		renderer.materialPreview_->textureID_ = 0;
	}
}

// ----------------------------------------------------------------------------

void Render_DebugPrintFrameNumber()
{
	printf("Frame #: %d\n", g_renderFrameCount);
}

// ----------------------------------------------------------------------------

void Render_SetWorldBounds(const glm::ivec3& worldMin, const glm::ivec3& worldMax)
{
	renderer.worldMin_ = worldMin;
	renderer.worldMax_ = worldMax;
}

