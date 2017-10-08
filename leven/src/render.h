#ifndef		HAS_RENDER_H_BEEN_INCLUDED
#define		HAS_RENDER_H_BEEN_INCLUDED

#include	"render_types.h"
#include	"render_debug.h"
#include	"render_mesh.h"
#include	"render_actor.h"

#include	"materials.h"

#include	<string>
#include	<glm/glm.hpp>
#include	<stdint.h>
#include	<vector>

// ----------------------------------------------------------------------------

struct ViewParams
{
	float		nearDistance, farDistance, fov, aspectRatio;
};

bool Render_Initialise(
	const int width, 
	const int height, 
	const bool useShadows, 
	const int shadowMapSize,
	const ViewParams& viewParams,
	MaterialSet& materials);

void Render_Reset();
void Render_Shutdown();

bool Render_DispatchCommands();

void Render_FrameBegin();
void Render_FrameAddMesh(RenderMesh* mesh);
void Render_FrameAddActorMesh(RenderMesh* mesh);
void Render_DrawFrame();
void Render_FrameEnd(int* numTriangles);

void Render_SetWorldBounds(const glm::ivec3& worldMin, const glm::ivec3& worldMax);

void Render_ToggleWireframe();
void Render_ToggleShadowView();
void Render_SetDrawUI(const bool enable);
void Render_ToggleLighting();
void Render_ToggleNormals();
void Render_RandomLightDir();

void Render_UpdatePreviewMaterial(const int materialID);

void Render_UnprojectPoint(const glm::ivec2& point, glm::vec3& worldspaceNear, glm::vec3& worldspaceFar);
const glm::mat4& Render_GetProjectionMatrix();
const glm::mat4& Render_GetWorldViewMatrix();
int Render_GetScreenWidth();
int Render_GetScreenHeight();

void Render_PrintStats();
void Render_DebugPrintFrameNumber();


#endif	//	HAS_RENDER_H_BEEN_INCLUDED

