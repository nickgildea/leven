#ifndef		HAS_GUI_H_BEEN_INCLUDED
#define		HAS_GUI_H_BEEN_INCLUDED

#include	"sdl_wrapper.h"
#include	<glm/glm.hpp>

struct GUIOptions
{
	bool		showOptions = true;
	bool		showOverlay = true;
	bool		regenerateVolume = false;
	u32			noiseSeed = 0xd33db33f;
	int			worldBrickCountXZ = 2;
};

struct GUIFrameInfo
{
	glm::vec3	position;
	int			numTriangles = 0;
	int			numVisibleNodes = 0;
};

void GUI_Initialise(SDL_Window* window);
void GUI_Shutdown();
void GUI_DrawFrame(GUIOptions* options, const GUIFrameInfo& frameInfo);
void GUI_ProcessEvent(SDL_Event* event);

#endif	//	HAS_GUI_H_BEEN_INCLUDED
