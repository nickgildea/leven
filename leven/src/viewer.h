#ifndef		HAS_VIEWER_H_BEEN_INCLUDED
#define		HAS_VIEWER_H_BEEN_INCLUDED

#include	"volume.h"

enum EditMode
{
	EditMode_Disabled,
	EditMode_CSG,
	EditMode_SpawnRigidBody,
};

enum ViewerMode
{
	ViewerMode_Clipmap,
	ViewerMode_Collision,
};

bool		Viewer_Initialise(const int worldBrickCountXZ, const glm::mat4& projectionMatrix, const int numMaterials);
void		Viewer_Shutdown();

void		Viewer_Update(const float deltaT, const ViewerMode viewerMode);

void		Viewer_ToggleEnableEdits();
void		Viewer_NextEditMode();

void		Viewer_IncreaseBrushSize();
void		Viewer_DecreaseBrushSize();
void		Viewer_ToggleSnapToGrid();

void		Viewer_UpdateBrushPosition();
void		Viewer_CycleBrushShape();
void		Viewer_CycleBrushMaterial(const bool cycleForwards);
void		Viewer_SelectNextBrush();
void		Viewer_EditVolume(const bool isAdditionOperation);
void		Viewer_CycleLockedAxis();
void		Viewer_ResetBrush();
void		Viewer_ToggleDrawSeams();
void		Viewer_ToggleEnableEdits();
void		Viewer_ToggleLockLOD();
void		Viewer_ToggleLoadEnabled();
void		Viewer_ToggleRotateBrush();

void Viewer_PlayerJump();
void Viewer_TogglePlayerNoClip();

const int CSGBrushShape_Cube = 0;
const int CSGBrushShape_Sphere = 1;
const int CSGBrushShape_Max = 2;

const int MIN_BRUSH_SIZE = 2 * LEAF_SIZE_SCALE;
const int MAX_BRUSH_SIZE = CLIPMAP_LEAF_SIZE - MIN_BRUSH_SIZE;

const int EditSnapMode_Off = 0;
const int EditSnapMode_Grid = 1;
const int EditSnapMode_FollowCamera = 2;

struct EditContext
{
	int				editMode = EditMode_Disabled;
	int				snapMode = EditSnapMode_Grid;
	ivec3			brushSize { MIN_BRUSH_SIZE };
	ivec3			brushPosition;
	vec3			brushNormal;
	RenderShape		brushShape = RenderShape_Cube;
	int				brushMaterial = 0;
};

EditContext Viewer_GetEditContextState();
void Viewer_UpdateEditContext(const EditContext& ctx);

#endif	//	HAS_VIEWER_H_BEEN_INCLUDED


