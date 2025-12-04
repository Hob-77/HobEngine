#include "ui/ImGuiLayer.h"
#include "core/Window.h"
#include "core/EngineTime.h"
#include "input/Input.h"
#include "core/Logger.h"
#include "renderer/opengl/GLDebugRenderer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace Engine
{

	void ImGuiLayer::init(Window* window)
	{
		if (m_initialized)
		{
			LOG_WARN("ImGuILayer already initialized");
			return;
		}

		LOG_INFO("Initializing ImGui Layer");

		m_window = window;

		// Create ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		// Configure ImGui
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation

		// ImGui style dark theme
		ImGui::StyleColorsDark();

		// Initialize SDL3 backend
		ImGui_ImplSDL3_InitForOpenGL(window->getNativeWindow(), window->getGLContext());

		// Initialize OpenGl3 backend
		const char* glsl_version = "#version 460";
		ImGui_ImplOpenGL3_Init(glsl_version);

		m_initialized = true;
		LOG_INFO("ImGui Layer initialized successfully");
	}

	void ImGuiLayer::shutdown()
	{
		if (!m_initialized)
		{
			return;
		}

		LOG_INFO("Shutting down ImGui Layer");

		// Shutdown backends in reverse order of initialization
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();

		// Destroy ImGui context
		ImGui::DestroyContext();

		m_initialized = false;
		m_window = nullptr;

		LOG_INFO("ImGui Layer shutdown complete");
	}

	void ImGuiLayer::beginFrame()
	{
		if (!m_initialized)
		{
			return;
		}

		// Tell backends to start a new frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();

		// Begin ImGui frame
		ImGui::NewFrame();
	}

	void ImGuiLayer::render()
	{
		if (!m_initialized)
		{
			return; // Can't render if not initialized
		}

		// Render debug UI windows
		if (m_showDebugUI)
		{
			renderDebugUI();
		}

		// Render demo window if enabled
		if (m_showDemoWindow)
		{
			ImGui::ShowDemoWindow(&m_showDemoWindow);
		}

		// Finalize ImGui frame and render
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void ImGuiLayer::processEvent(const SDL_Event& event)
	{
		if (!m_initialized)
		{
			return; // Can't process events if not initialized
		}

		// Feed event to ImGui's SDL3 backend
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	bool ImGuiLayer::wantsCaptureMouse() const
	{
		if (!m_initialized)
		{
			return false; // If not initialized, ImGui isn't capturing anything
		}

		return ImGui::GetIO().WantCaptureMouse;
	}

	bool ImGuiLayer::wantsCaptureKeyboard() const
	{
		if (!m_initialized)
		{
			return false; // If not initialized, ImGui isn't capturing anything
		}

		return ImGui::GetIO().WantCaptureKeyboard;
	}

	void ImGuiLayer::renderDebugUI()
	{
		ImGui::Begin("Engine Debug", &m_showDebugUI);

		// === PERFORMANCE ===
		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("FPS: %d", EngineTime::getFPS());
			ImGui::Text("Frame Time: %.2f ms", EngineTime::getFrameTime());
			ImGui::Text("Delta Time: %.4f s", EngineTime::getDeltaTime());
		}

		// === INPUT ===
		if (ImGui::CollapsingHeader("Input"))
		{
			Engine::vec2 mousePos = Input::getMousePosition();
			Engine::vec2 mouseDelta = Input::getMouseDelta();
			ImGui::Text("Mouse Pos: (%.0f, %.0f)", mousePos.x, mousePos.y);
			ImGui::Text("Mouse Delta: (%.0f, %.0f)", mouseDelta.x, mouseDelta.y);
			ImGui::Text("ImGui Mouse Capture: %s", wantsCaptureMouse() ? "Yes" : "No");
			ImGui::Text("ImGui Keyboard Capture: %s", wantsCaptureKeyboard() ? "Yes" : "No");
		}

		// === DEBUG RENDERER ===
		if (ImGui::CollapsingHeader("Debug Renderer"))
		{
			bool debugDepthTest = GLDebugRenderer::staticGetDepthTest();
			if (ImGui::Checkbox("Depth Test", &debugDepthTest))
			{
				GLDebugRenderer::staticSetDepthTest(debugDepthTest);
			}

			float lineWidth = GLDebugRenderer::staticGetLineWidth();
			if (ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 5.0f))
			{
				GLDebugRenderer::staticSetLineWidth(lineWidth);
			}
		}

		ImGui::Separator();

		// === BUTTONS ===
		if (ImGui::Button(m_showDemoWindow ? "Hide Demo" : "Show Demo"))
		{
			m_showDemoWindow = !m_showDemoWindow;
		}

		ImGui::SameLine();

		if (ImGui::Button("Close"))
		{
			m_showDebugUI = false;
		}

		ImGui::End();
	}

}