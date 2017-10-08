#include	<string>
#include	<string.h>
#include	<stdio.h>
#include	<vector>
#include	<memory>
#include	<thread>
#include	<random>
#include	<algorithm>

#include	<CL/cl.hpp>
#include	"sdl_wrapper.h"
#include	<glm/glm.hpp>
#include	<glm/ext.hpp>
#include	<Remotery.h>

#include	"camera.h"
#include	"resource.h"
#include	"log.h"
#include	"render.h"
#include	"viewer.h"
#include	"threadpool.h"
#include	"config.h"
#include	"compute.h"
#include	"timer.h"
#include	"materials.h"
#include	"gui.h"

Config g_config;



// ----------------------------------------------------------------------------

bool g_grabbed = false;

bool IsInputGrabbed()
{
	return g_grabbed;
}

// ----------------------------------------------------------------------------

void GrabInput(SDL_Window* window, const bool grab)
{
	g_grabbed = grab;

	SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	SDL_ShowCursor(grab ? 0 : 1);
}

// ----------------------------------------------------------------------------

// TODO this is a special kind of shit
void HandleKeyPress(
	SDL_Window* window, 
	SDL_Event& e, 
	CameraInput& input, 
	bool& rebuildVolume, 
	bool& quit, 
	ViewerMode& viewerMode)
{
	quit = false;

	SDL_KeyboardEvent* event = &e.key;
	GrabInput(window, true);

	if (event->type == SDL_KEYDOWN)
	{
		// TODO handle this with the camera properly
		switch (event->keysym.sym)
		{
			case SDLK_w:
			{
				input.speed.z = -1.f;
				break;
			}

			case SDLK_s:
			{
				input.speed.z = 1.f;
				break;
			}

			case SDLK_a:
			{
				input.speed.x = -1.f;
				break;
			}
			
			case SDLK_d:
			{
				input.speed.x = 1.f;
				break;
			}
			
			case SDLK_z:
			{
				input.speed.y = 1.f;
				break;
			}

			case SDLK_x:
			{
				input.speed.y = -1.f;
				break;
			}

			case SDLK_SPACE:
			{
				Viewer_PlayerJump();
				break;
			}
		}
	}
	else
	{
		switch (event->keysym.sym)
		{
			case SDLK_w:
			case SDLK_s:
			{
				input.speed.z = 0.f;
				break;
			}

			case SDLK_a:
			case SDLK_d:
			{
				input.speed.x = 0.f;
				break;
			}
			
			case SDLK_z:
			case SDLK_x:
			{
				input.speed.y = 0.f;
				break;
			}

			case SDLK_F10:
			{
				Viewer_TogglePlayerNoClip();
				break;
			}

			case SDLK_1:
			{
				Viewer_ResetBrush();
				break;
			}

			case SDLK_2:
			{
				Viewer_IncreaseBrushSize();
				break;
			}

			case SDLK_3:
			{
				Viewer_DecreaseBrushSize();
				break;
			}

			case SDLK_F1:
			{
				Render_ToggleWireframe();
				break;
			}

			case SDLK_F2:
			{
				Viewer_ToggleEnableEdits();
				break;
			}

			case SDLK_F3:
			{
				Render_ToggleLighting();
				break;
			}

			case SDLK_F4:
			{
				Render_ToggleNormals();
				break;
			}

			case SDLK_F5:
			{
				Render_RandomLightDir();
				break;
			}

			case SDLK_F6:
			{
				if (viewerMode == ViewerMode_Clipmap)
					viewerMode = ViewerMode_Collision;
				else
					viewerMode = ViewerMode_Clipmap;
				
				break;
			}

			case SDLK_F7:
			{
				Viewer_ToggleLockLOD();
				break;
			}

			case SDLK_F8:
			{
				Viewer_ToggleLoadEnabled();
				break;
			}

			case SDLK_r:
			{
				Viewer_SelectNextBrush();
				break;
			}

			case SDLK_F9:
			{
				rebuildVolume = true;
				break;
			}

			case SDLK_F12:
			{
				quit = true;
				break;
			}

			case SDLK_TAB:
			{
				Viewer_NextEditMode();
				break;
			}

			case SDLK_ESCAPE:
			{
				GrabInput(window, false);
				break;
			}
		}
	}
}


// ----------------------------------------------------------------------------

void OnMouseButtonDown(SDL_Window* window, SDL_Event& event)
{
	GrabInput(window, true);

	// TODO these are not constants
	const int width = g_config.windowWidth;
	const int height = g_config.windowHeight;

	// sdl is (0,0) upper left, opengl is (0,0) lower left corner
	const int x = event.button.x;
	const int y = height - event.button.y;

	if (event.type == SDL_MOUSEBUTTONDOWN &&
		(event.button.button == SDL_BUTTON_LEFT ||
		event.button.button == SDL_BUTTON_RIGHT))
	{
		Viewer_EditVolume(event.button.button == SDL_BUTTON_LEFT);
	}
	else if (event.type & SDL_MOUSEWHEEL)
	{
		const bool forwards = event.wheel.y >= 0;
		Viewer_CycleBrushMaterial(forwards);
	}
}

// ----------------------------------------------------------------------------

void OnMouseMotion(
	SDL_Window* window, 
	const SDL_Event& event, 
	CameraInput& input, 
	const int centreX, 
	const int centreY,
	const bool mouseDownAllowed)
{
	if (IsInputGrabbed())
	{
		const SDL_MouseMotionEvent* mm = &event.motion;

		const float YAW_SCALE = 0.5f;
		const float PITCH_SCALE = 0.5f;
		if (abs(mm->xrel) <= 30)
		{
			input.yaw = YAW_SCALE * mm->xrel;
		}

		if (abs(mm->yrel) <= 30)
		{
			input.pitch = PITCH_SCALE * mm->yrel;
		}

		Viewer_UpdateBrushPosition();
		SDL_WarpMouseInWindow(window, centreX, centreY);

		if (mouseDownAllowed)
		{
			if (event.motion.state & SDL_BUTTON_LMASK)
			{
				Viewer_EditVolume(true);
			}
			else if (event.motion.state & SDL_BUTTON_RMASK)
			{
				Viewer_EditVolume(false);
			}
		}
	}
}

// ----------------------------------------------------------------------------

int main(int argc, char** argv)
{
	const std::string terrainPath = argc >= 2 ? argv[1] : "";

	if (!Config_Load(g_config, "default.cfg"))
	{
		printf("Unable to load default.cfg!\n");
		return EXIT_FAILURE;
	}

	if (!LogInit())
	{
		return -1;
	}

	ThreadPool_Initialise(g_config.threadpoolCount);

	Remotery* rmt = nullptr;
	if (rmt_CreateGlobalInstance(&rmt) != RMT_ERROR_NONE)
	{
		printf("Unable to start Remotery!\n");
		return EXIT_FAILURE;
	}

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		LogPrintf("Error: unable to initialise SDL: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	auto windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
	if (g_config.fullscreen)
	{
		windowFlags |= SDL_WINDOW_FULLSCREEN;
	}

#ifdef _DEBUG
	const char* windowTitle = "Leven (Debug)";
#elif defined(TESTING)
	const char* windowTitle = "Leven (Testing)";
#else
	const char* windowTitle = "Leven (Release)";
#endif


	SDL_Window* window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		g_config.windowWidth, g_config.windowHeight, windowFlags);
	if (window == NULL)
	{
		return -1;
	}

	SDL_GLContext defaultContext = SDL_GL_CreateContext(window);
//	SDL_EnableKeyRepeat(150, SDL_DEFAULT_REPEAT_INTERVAL);
	GrabInput(window, false);

	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		LogPrintf("Error: %s\n", glewGetErrorString(err));
		return -1;
	}

	printf("OpenGL status (using GLEW): %s\n", glewGetString(GLEW_VERSION));
	printf("OpenGL version: %s\n", glGetString(GL_VERSION));
	printf("OpenGL shading version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	GUIOptions guiOptions;
	guiOptions.noiseSeed = g_config.noiseSeed;

	GUIFrameInfo guiFrameInfo;

	ViewParams viewParams;
	viewParams.fov = 60.f;		
	viewParams.aspectRatio = (float)g_config.windowWidth / (float)g_config.windowHeight;
	viewParams.nearDistance = 0.1f;
	viewParams.farDistance = 16000.f;

	MaterialSet materials;
	materials.addMaterial("assets/stone0.png", "assets/grass1.png", "assets/stone0.png");
	materials.addMaterial("assets/stone1.png");
	materials.addMaterial("assets/stone2.png");
	materials.addMaterial("assets/redbrick.png");

	if (!Render_Initialise(g_config.windowWidth, g_config.windowHeight, 
		g_config.useShadows, g_config.shadowMapSize, viewParams, materials))
	{
		printf("Render_Initialise: a fatal error occured\n");
		return EXIT_FAILURE;
	}

	Camera_Initialise();
	const vec3 cameraStartPosition(0.f, 3000.f, 0.f);
	Camera_SetPosition(cameraStartPosition);

	const int error = Compute_Initialise(guiOptions.noiseSeed, 0, 2);
	if (error)
	{
		printf("Compute_Initialise: a fatal error occured: %d\n", error);
		return EXIT_FAILURE;
	}

	// use a wider FOV for the culling so clipmap nodes just offscreen are still selected
	glm::mat4 volumeProjection = glm::perspective(90.f, 
		viewParams.aspectRatio, viewParams.nearDistance, viewParams.farDistance);

	Viewer_Initialise(guiOptions.worldBrickCountXZ, volumeProjection, materials.size());
	GUI_Initialise(window);

	printf("----------------------------------------------------------\n");
	printf("Controls:\n");
	printf("\n");
	printf("Escape: release mouse\n");
	printf("W, A, S, D: movement\n");
	printf("\n");
	printf("Left click: add\n");
	printf("Right click: remove\n");
	printf("\n");
	printf("1: reset brush size\n");
	printf("2: increase brush size\n");
	printf("3: decrease brush size\n");
	printf("TAB: toggle CSG edits / physics objects spawn\n");
	printf("F1: toggle wireframe\n");
	printf("F2: toggle edit mode (default=off)\n");
	printf("F3: show lightmap\n");
	printf("F4: show normals\n");
	printf("F5: random sun direction\n");
	printf("F6: toggle draw mode (render world / physics world)\n");
	printf("F7: toggle lock LOD\n");
	printf("F8: toggle dynamic load enabled\n");
	printf("F10: toggle flymode for camera\n");
	printf("----------------------------------------------------------\n");

	Uint32 startTime = SDL_GetTicks();
	Uint32 prevTime = startTime;
	Uint32 prevVoxelTime = startTime;
	uint32_t maxVoxelT = 0;
	int32_t numVoxelUpdates = 0;
	int32_t sumVoxelT = 0;

	CameraInput cameraInput;

	Uint32 nextMouseDownAllowed = 0;
	const Uint32 MOUSE_DOWN_DISALLOW = 17 * 2;

	SDL_GL_SetSwapInterval(1);
	Render_DispatchCommands();

	bool useSDLInput = false;
	ViewerMode viewerMode = ViewerMode_Clipmap;

	SDL_Event event;
	bool quit = false;
	while (!quit)
	{
		const Uint32 currentT = SDL_GetTicks();
		const auto deltaTicks = (currentT - prevTime);
		const float deltaT = deltaTicks / 1000.f;
		if (deltaT <= (1.f/60.f))
		{
//			continue;
		}

		prevTime = currentT;

		bool rebuildVolume = false;
		cameraInput.yaw = cameraInput.pitch = 0.f;

		while (SDL_PollEvent(&event))
		{
			rmt_ScopedCPUSample(PollEvents);

			quit = event.type == SDL_QUIT;

			if (!useSDLInput)
			{
				GUI_ProcessEvent(&event);
				continue;
			}

			switch (event.type)
			{
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					HandleKeyPress(window, event, cameraInput, rebuildVolume, quit, viewerMode);
					
					if (event.type == SDL_KEYUP && !IsInputGrabbed())
					{
						guiOptions.showOptions = true;
					}

					break;
				}

				case SDL_MOUSEBUTTONDOWN:
				{
					if (currentT >= nextMouseDownAllowed)
					{
						OnMouseButtonDown(window, event);
						nextMouseDownAllowed = currentT + MOUSE_DOWN_DISALLOW;
					}
					
					break;
				}

				case SDL_MOUSEBUTTONUP:
				{
					Viewer_UpdateBrushPosition();
					break;
				}

				case SDL_MOUSEWHEEL:
				{
					OnMouseButtonDown(window, event);
					break;
				}

				case SDL_MOUSEMOTION:
				{
					bool allowMouseDown = false;
					if (currentT >= nextMouseDownAllowed)
					{
						allowMouseDown = true;
						nextMouseDownAllowed = currentT + MOUSE_DOWN_DISALLOW;
					}

					OnMouseMotion(window, event, cameraInput, g_config.windowWidth / 2, g_config.windowHeight / 2, allowMouseDown);
					break;
				}
			}
		}

		{
			rmt_ScopedCPUSample(RenderUpdate);
			Render_FrameBegin();

			Viewer_Update(deltaT, viewerMode);
			Camera_Update(deltaT, cameraInput);

			Render_FrameEnd(&guiFrameInfo.numTriangles);
		}

		Render_DispatchCommands();
		Render_DrawFrame();

		guiFrameInfo.position = Camera_GetPosition();
		GUI_DrawFrame(&guiOptions, guiFrameInfo);
		if (!useSDLInput && !guiOptions.showOptions)
		{
			// just about to disable the GUI, warp the mouse back to prevent huge movements
			GrabInput(window, true);
		}

		useSDLInput = !guiOptions.showOptions;

		SDL_GL_SwapWindow(window);

		if (guiOptions.regenerateVolume)
		{
			Viewer_Shutdown();
			Render_Reset();

			if (int error = Compute_SetNoiseSeed(guiOptions.noiseSeed) < 0)
			{
				printf("Error: %d couldn't reset noise seed\n", error);
			}
			
//			Camera_SetPosition(cameraStartPosition);
			Viewer_Initialise(guiOptions.worldBrickCountXZ, volumeProjection, materials.size());
		}
	}

	GUI_Shutdown();

	Compute_Shutdown();

	Viewer_Shutdown(); 
	Render_Shutdown();

	ThreadPool_Destroy();

	SDL_GL_DeleteContext(defaultContext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	LogShutdown();
	rmt_DestroyGlobalInstance(rmt);

#ifdef _DEBUG	
	_CrtDumpMemoryLeaks();
#endif // _DEBUG

	return 0;
}

