#include	"viewer.h"

#include	"volume_constants.h"
#include	"volume.h"
#include	"resource.h"
#include	"render.h"
#include	"threadpool.h"
#include	"camera.h"		
#include	"aabb.h"
#include	"octree.h"
#include	"compute.h"
#include	"timer.h"
#include	"contour_constants.h"
#include	"log.h"
#include	"frustum.h"
#include	"actors.h"
#include	"random.h"

#include	<vector>
#include	<algorithm>

#include	<glm/glm.hpp>
#include	<glm/ext.hpp>
using glm::ivec4;
using glm::ivec3;
using glm::ivec2;
using glm::vec4;
using glm::vec3;
using glm::vec2;

Volume g_volume;

// ----------------------------------------------------------------------------

// TODO globals bad :|
glm::mat4 g_projection;

bool g_lockLOD = false;
bool g_loadEnabled = true;

std::vector<uint32_t> g_brushMaterials;
EditContext g_editContext;
int g_brushDrawBuffer = -1;
int g_debugRayCastBuffer = -1;


// ----------------------------------------------------------------------------

bool Viewer_Initialise(const int worldBrickCountXZ, const glm::mat4& projection, const int numMaterials)
{
	printf("Viewer_Initialise\n");

	for (int i = 0; i < numMaterials; i++)
	{
		g_brushMaterials.push_back(i);
	}

	g_projection = projection;

	Render_UpdatePreviewMaterial(g_brushMaterials[g_editContext.brushMaterial]);
	g_brushDrawBuffer = Render_AllocDebugDrawBuffer();
	g_debugRayCastBuffer = Render_AllocDebugDrawBuffer();

	const ivec3 worldOrigin(0);
	const int BRICK_SIZE = 8;
	const int worldSizeXZ = worldBrickCountXZ * BRICK_SIZE * CLIPMAP_LEAF_SIZE;
	const int worldSizeY = 4 * BRICK_SIZE * CLIPMAP_LEAF_SIZE;
	const ivec3 worldSize(worldSizeXZ, worldSizeY, worldSizeXZ);
	const AABB worldBounds(worldOrigin - (worldSize / 2), worldOrigin + (worldSize / 2));

	Render_SetWorldBounds(worldBounds.min, worldBounds.max);
	Physics_Initialise(worldBounds);
	g_volume.initialise(ivec3(Camera_GetPosition()), worldBounds);
	Actor_Initialise(worldBounds);

	Physics_SpawnPlayer(vec3(-500.f, 4000.f, -500.f));

	return true;
}

// ----------------------------------------------------------------------------

void Viewer_Shutdown()
{
	Actor_Shutdown();
	g_volume.destroy();
	Physics_Shutdown();

	Render_FreeDebugDrawBuffer(&g_brushDrawBuffer);
	Render_FreeDebugDrawBuffer(&g_debugRayCastBuffer);
}

// ----------------------------------------------------------------------------

const ivec3 SnapPositionToBrushCentre(const ivec3& position)
{
	const ivec3 halfBrushSize = g_editContext.brushSize / 2;

	// round the position toward the next half size position, when in quadrants with -ve values
	// we need to snap in the opposite direction, so control the direction by multiplying by sign()
	return ivec3(
		position.x + (glm::sign(position.x) * halfBrushSize.x - (position.x % g_editContext.brushSize.x)),
		position.y + (glm::sign(position.y) * halfBrushSize.y - (position.y % g_editContext.brushSize.y)),
		position.z + (glm::sign(position.z) * halfBrushSize.z - (position.z % g_editContext.brushSize.z))
	);
}

// ----------------------------------------------------------------------------

const vec3 GetNormalDominantAxis(const vec3& nodeNormal)
{
	// use the dominant axis of the node
	vec3 dir;
	const vec3 absNormal = glm::abs(nodeNormal);
	if (absNormal.y >= absNormal.x)
	{
		if (absNormal.y >= absNormal.z)
		{
			dir.y = nodeNormal.y;
		}
		else
		{
			dir.z = nodeNormal.z;
		}
	}
	else
	{
		if (absNormal.x >= absNormal.z)
		{
			dir.x = nodeNormal.x;
		}
		else
		{
			dir.z = nodeNormal.z;
		}
	}

	return glm::normalize(dir);
}

// ----------------------------------------------------------------------------

const ivec3 CalculateBrushPosition(const ivec3& nodePosition, const vec3& nodeNormal)
{
	ivec3 brushPos = nodePosition;

	if (g_editContext.snapMode == EditSnapMode_Grid)
	{
		const float halfBrushSize = g_editContext.brushSize[0] / 2.f;
		const vec3 dir = -1.f * Camera_GetForward();
		brushPos = SnapPositionToBrushCentre(brushPos + ivec3((dir * halfBrushSize)));
	}

	return brushPos;
}

// ----------------------------------------------------------------------------

void Viewer_UpdateBrushPosition()
{
	if (g_editContext.editMode == EditMode_Disabled)
	{
		return;
	}

	const vec3 hitPosition = Physics_LastHitPosition();
	const vec3 hitNormal = Physics_LastHitNormal();

	g_editContext.brushPosition = CalculateBrushPosition(ivec3(hitPosition), hitNormal);	
	g_editContext.brushNormal = hitNormal;

#if 0
	static RenderDebugCmdArray cmds;
	cmds.push_back({Line, Red, 1.f, hitPosition, hitPosition + (10.f * hitNormal)});
	cmds.push_back({Line, Green, 1.f, hitPosition, glm::normalize(Camera_GetPosition() - hitPosition) * 10.f + hitPosition});
	Render_SetDebugDrawCmds(g_debugRayCastBuffer, cmds);
	if (cmds.size() > 1000) cmds.clear();
#endif

	const int width = Render_GetScreenWidth();
	const int height = Render_GetScreenHeight();

	vec3 near, far;
	Render_UnprojectPoint(ivec2(width / 2, height / 2), near, far);
	Physics_CastRay(near, near + glm::normalize(far - near) * 10000.f);
}

// ----------------------------------------------------------------------------

void Viewer_EditVolume(const bool isAdditionOperation)
{
	if (g_editContext.editMode == EditMode_SpawnRigidBody)
	{
		const vec3 spawnPosition = vec3(g_editContext.brushPosition + ivec3(0, 100, 0));

		const ActorShape shapes[2] = { ActorShape_Cube, ActorShape_Sphere };
		Actor_Spawn(shapes[RandomS32(0, 1)], RandomColour(), RandomF32(16.f, 64.f), spawnPosition);
	}
	else if (g_editContext.editMode == EditMode_CSG)
	{
		vec3 offset;
		if (g_editContext.snapMode == EditSnapMode_Grid && isAdditionOperation)
		{
			const vec3 dir = GetNormalDominantAxis(Camera_GetForward());
			offset = dir * vec3(g_editContext.brushSize);
		}
		
		const vec3 origin = offset + vec3(g_editContext.brushPosition);
		const vec3 dimensions(vec3(g_editContext.brushSize) / 2.f);

		g_volume.applyCSGOperation(origin, vec3(g_editContext.brushSize), 
			g_editContext.brushShape, g_brushMaterials[g_editContext.brushMaterial], 
			g_editContext.snapMode == EditSnapMode_FollowCamera, isAdditionOperation);
	}
}

// ----------------------------------------------------------------------------

void Viewer_Update(const float deltaT, const ViewerMode viewerMode)
{
	const vec3& currentPos = Camera_GetPosition();

	const glm::mat4& mv = Render_GetWorldViewMatrix();
	Frustum frustum = BuildFrustum(g_projection * mv);

	const float UPDATE_LOD_TICK = 1.f / 5.f;
	static float updateLODTimer = 0.f;

	if (!g_lockLOD && updateLODTimer >= UPDATE_LOD_TICK)
	{
		g_volume.processCSGOperations();
		g_volume.updateChunkLOD(currentPos, frustum);
		updateLODTimer = 0.f;
	}

	updateLODTimer += deltaT;

	Actor_Update();

	const std::vector<RenderMesh*> meshes = viewerMode == ViewerMode_Clipmap ? 
		g_volume.findVisibleMeshes(frustum) : Physics_GetRenderData(frustum);
	for (RenderMesh* mesh: meshes)
	{
		Render_FrameAddMesh(mesh);
	}

	const std::vector<RenderMesh*> actorMeshes = Actor_GetVisibleActors(frustum);
	for (RenderMesh* mesh: actorMeshes)
	{
		Render_FrameAddActorMesh(mesh);
	}

	// Also account for the grid offset so the preview matches with the operation!
	const vec3 brushPos = vec3(g_editContext.brushPosition) + vec3(0.5f * LEAF_SIZE_SCALE);
	const vec3 halfSize = (vec3(g_editContext.brushSize) / 2.f);

	RenderDebugCmdBuffer renderCmds;
	if (g_editContext.editMode != EditMode_Disabled)
	{
		const vec3 colour = g_editContext.editMode == EditMode_CSG ? RenderColour_Red : RenderColour_Green;
		if (g_editContext.brushShape == RenderShape_Cube)
		{
			renderCmds.addCube(colour, 0.2f, brushPos - halfSize, g_editContext.brushSize.x);
		}
		else
		{
			renderCmds.addSphere(colour, 0.2f, brushPos, halfSize.x / 2.f);
		}
	}

	// always create so the old preview doesn't remain
	Render_SetDebugDrawCmds(g_brushDrawBuffer, renderCmds);
}

// ----------------------------------------------------------------------------

void Viewer_IncreaseBrushSize()
{
	g_editContext.brushSize = glm::min(g_editContext.brushSize + ivec3(8), ivec3(MAX_BRUSH_SIZE));
	printf("Brush size: %d %d %d\n", g_editContext.brushSize.x, g_editContext.brushSize.y, g_editContext.brushSize.z);
}

// ----------------------------------------------------------------------------

void Viewer_DecreaseBrushSize()
{
	g_editContext.brushSize = glm::max(g_editContext.brushSize - ivec3(8), ivec3(MIN_BRUSH_SIZE));
	printf("Brush size: %d %d %d\n", g_editContext.brushSize.x, g_editContext.brushSize.y, g_editContext.brushSize.z);
}

// ----------------------------------------------------------------------------

void Viewer_CycleBrushMaterial(const bool cycleForwards)
{
	if (cycleForwards)
	{
		g_editContext.brushMaterial++;
	}
	else
	{
		g_editContext.brushMaterial--;
	}

	if (g_editContext.brushMaterial < 0)
	{
		g_editContext.brushMaterial = g_brushMaterials.size() - 1;
	}
	else if (g_editContext.brushMaterial >= g_brushMaterials.size())
	{
		g_editContext.brushMaterial = 0;
	}

	Render_UpdatePreviewMaterial(g_brushMaterials[g_editContext.brushMaterial]);
}

// ----------------------------------------------------------------------------

void Viewer_SelectNextBrush()
{
	// ugh
	g_editContext.brushShape = RenderShape((g_editContext.brushShape + 1) % 2);
}

// ----------------------------------------------------------------------------

void Viewer_ToggleLockLOD()
{
	g_lockLOD = !g_lockLOD;
	printf("Lock LOD: %s\n", g_lockLOD ? "True" : "False");
}

// ----------------------------------------------------------------------------

void Viewer_ToggleLoadEnabled()
{
	g_loadEnabled = !g_loadEnabled;
	printf("Load enabled: %s\n", g_loadEnabled ? "True" : "False");
}

// ----------------------------------------------------------------------------

void Viewer_ResetBrush()
{
	g_editContext.brushSize = ivec3(MIN_BRUSH_SIZE);
}

// ----------------------------------------------------------------------------

void Viewer_ToggleEnableEdits()
{
	if (g_editContext.editMode == EditMode_Disabled)
	{
		g_editContext.editMode = EditMode_CSG;
	}
	else
	{
		g_editContext.editMode = EditMode_Disabled;
	}

	Render_SetDrawUI(g_editContext.editMode != EditMode_Disabled);
}

// ----------------------------------------------------------------------------

void Viewer_NextEditMode()
{
	if (g_editContext.editMode == EditMode_CSG)
	{
		g_editContext.editMode = EditMode_SpawnRigidBody;
	}
	else
	{
		g_editContext.editMode = EditMode_CSG;
	}
}

// ----------------------------------------------------------------------------

EditContext Viewer_GetEditContextState()
{
	return g_editContext;
}

// ----------------------------------------------------------------------------

void Viewer_UpdateEditContext(const EditContext& ctx)
{
	g_editContext = ctx;
}

// ----------------------------------------------------------------------------

void Viewer_PlayerJump()
{
	Physics_PlayerJump();
}

// ----------------------------------------------------------------------------

void Viewer_TogglePlayerNoClip()
{
	Physics_TogglePlayerNoClip();
}

// ----------------------------------------------------------------------------