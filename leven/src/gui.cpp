#include	"gui.h"

#include	"imgui.h"
#include	"imgui_handlers.h"
#include	"viewer.h"
#include	"options.h"
#include	<random>

// ----------------------------------------------------------------------------

SDL_Window* g_guiWindow = nullptr;

// ----------------------------------------------------------------------------

void GUI_Initialise(SDL_Window* window)
{
	g_guiWindow = window;
	ImGui_ImplSdl_Init(g_guiWindow);
}

// ----------------------------------------------------------------------------

void GUI_Shutdown()
{
	g_guiWindow = nullptr;
	ImGui_ImplSdl_Shutdown();
}

// ----------------------------------------------------------------------------

void GUI_DrawFrame(GUIOptions* options, const GUIFrameInfo& frameInfo)
{
	EditContext ctx = Viewer_GetEditContextState();

	static std::mt19937 prng;
	static std::uniform_int_distribution<uint32_t> distribution(1 << 28, INT_MAX);

	ImGui::GetIO().MouseDrawCursor = false;
	ImGui_ImplSdl_NewFrame(g_guiWindow);
	SDL_ShowCursor(options->showOptions ? 1 : 0);

	if (options->showOptions)
	{
		ImGui::Begin("Options");

		if (ImGui::Button("Close Window")) { options->showOptions = false; }
		ImGui::SameLine();
		if (ImGui::Button("Toggle Overlay")) { options->showOverlay = !options->showOverlay; }

		if (ImGui::CollapsingHeader("Options"))
		{
			if (ImGui::TreeNode("Terrain"))
			{
				static char seed[9] = "";
				SDL_snprintf(seed, 9, "%x", options->noiseSeed);
				if (ImGui::InputText("Noise seed", seed, 9, 
					ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
				{
					char* end = nullptr;
					options->noiseSeed = SDL_strtol(seed, &end, 16);
				}

				ImGui::SliderInt("Size", &options->worldBrickCountXZ, 2, 16);

				ImGui::BeginGroup();
				if (ImGui::Button("Random Noise Seed"))
				{
					options->noiseSeed = distribution(prng);
				}

				ImGui::SameLine();
				options->regenerateVolume = ImGui::Button("Regenerate Volume");
				ImGui::SameLine();
				ImGui::EndGroup();
				
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Brush"))
			{
				int brushSize = ctx.brushSize[0];
				if (ImGui::SliderInt("Brush size", &brushSize, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE))
				{
					ctx.brushSize = ivec3(brushSize);
				}

				ImGui::BeginGroup();
				ImGui::Text("Snap mode:");
				ImGui::SameLine();
				ImGui::RadioButton("Off", &ctx.snapMode, EditSnapMode_Off);
				ImGui::SameLine();
				ImGui::RadioButton("Grid", &ctx.snapMode, EditSnapMode_Grid);
				ImGui::SameLine();
				ImGui::RadioButton("Follow camera", &ctx.snapMode, EditSnapMode_FollowCamera);
				ImGui::EndGroup();

				ImGui::TreePop();

				Viewer_UpdateEditContext(ctx);
			}

			if (ImGui::TreeNode("Mesh Simplification"))
			{
				auto& options = Options::get();
				ImGui::SliderFloat("Max error", &options.meshMaxError_, 0.f, 50.f);
				ImGui::SliderFloat("Max edge length", &options.meshMaxEdgeLen_, 0.5, 5.0);
				ImGui::SliderFloat("Max normal angle", &options.meshMinCosAngle_, 0, 1);
			}
		}

		ImGui::End();	
	}

	if (options->showOverlay)
	{
		ImGui::SetNextWindowPos(ImVec2(10,10));
		if (ImGui::Begin("Info", nullptr, ImVec2(250,0), 0.3f, 
			ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
			ImGui::Text("Position: %.1f %.1f %.1f", frameInfo.position.x, frameInfo.position.y, frameInfo.position.z);
			ImGui::Text("Triangles: %.1f k", frameInfo.numTriangles / 1000.f);
			ImGui::Text("Clipmap Nodes: %d\n", frameInfo.numVisibleNodes);
		}

		ImGui::End();
	}

	// Rendering
	ImGuiIO& io = ImGui::GetIO();
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	ImGui::Render();
}

void GUI_ProcessEvent(SDL_Event* event)
{
	ImGui_ImplSdl_ProcessEvent(event);
}

